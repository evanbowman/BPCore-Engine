#include "multi.h"
#include "gba.h"


#ifndef NULL
#define NULL 0
#endif


static inline void multi_register_serial_isr(void (*isr)(void))
{
    irqSet(IRQ_SERIAL, isr);
}


static inline void multi_register_timer2_isr(void (*isr)(void))
{
    irqSet(IRQ_TIMER2, isr);
}


static inline void multi_enable_timer2_irq(int enabled)
{
    if (enabled) {
        irqEnable(IRQ_TIMER2);
    } else {
        irqDisable(IRQ_TIMER2);
    }
}


static inline void multi_enable_serial_irq(int enabled)
{
    if (enabled) {
        irqEnable(IRQ_SERIAL);
    } else {
        irqDisable(IRQ_SERIAL);
    }
}


static int connection_mask;


static int multiplayer_is_master()
{
    return (REG_SIOCNT & (1 << 2)) == 0;
}


// These enums don't have anything to do with the gba hardware, they're just
// magic constants that we're using during startup to detect which gbas are want
// to connect.
enum {
    MULTI_DEVICE_READY = 0xAA,

    // The host (master) device will broadcast a start command when the host
    // player decides to start the multiplayer game. (e.g. when the host player
    // decides that enough players have connected, and presses a button, or
    // something).
    MULTI_DEVICE_START = 0xFF,
};


// Cache our device's multiplayer id.
static multi_PlayerId g_multi_id = multi_PlayerId_unknown;
static multi_DataCallback g_data_callback = NULL;
static multi_SendCallback g_send_callback = NULL;


volatile int sio_got_intr = 0;


static void multi_record_id()
{
    // NOTE: it's only safe to read the multiplayer id immediatly after a
    // transmission, otherwise, the register might contain a garbage value.

    switch ((REG_SIOCNT & (0x30)) >> 4) {
    case 0:
        g_multi_id = multi_PlayerId_host;
        break;

    case 1:
        g_multi_id = multi_PlayerId_p1;
        break;

    case 2:
        g_multi_id = multi_PlayerId_p2;
        break;

    case 3:
        g_multi_id = multi_PlayerId_p3;
        break;

    default:
        g_multi_id = multi_PlayerId_unknown;
        break;
    }
}


// This is just some boilerplate code for waiting on a serial interrupt, used
// while initializing the connection. We want to wait until the transmission is
// complete before reading serial registers.
static void multi_connect_serial_isr()
{
    sio_got_intr = 1;

    multi_record_id();
}


static inline void multi_tx_send()
{
    u16 output;
    g_send_callback(&output);
    REG_SIOMLT_SEND = output;
}


static void multi_connect_check_device_ready(int* connection_mask,
                                             u16 state,
                                             multi_PlayerId device_id,
                                             multi_ConnectedCallback callback)
{
    if (state == MULTI_DEVICE_READY && !(*connection_mask & device_id)) {
        *connection_mask |= device_id;
        callback(device_id, 1);
    } else if (state != MULTI_DEVICE_READY && *connection_mask & device_id) {
        *connection_mask &= ~device_id;
        callback(device_id, 0);
    }
}


static void multi_connect_check_devices(int* connection_mask,
                                        multi_ConnectedCallback callback)
{
    multi_connect_check_device_ready(connection_mask,
                                     REG_SIOMULTI0,
                                     multi_PlayerId_host,
                                     callback);

    multi_connect_check_device_ready(connection_mask,
                                     REG_SIOMULTI1,
                                     multi_PlayerId_p1,
                                     callback);

    multi_connect_check_device_ready(connection_mask,
                                     REG_SIOMULTI2,
                                     multi_PlayerId_p2,
                                     callback);

    multi_connect_check_device_ready(connection_mask,
                                     REG_SIOMULTI3,
                                     multi_PlayerId_p3,
                                     callback);
}


static void __attribute__((noinline)) busy_wait(unsigned max)
{
    for (unsigned i = 0; i < max; i++) {
        __asm__ volatile("" : "+g"(i) : :);
    }
}


static void multi_serial_init();


multi_PlayerId multi_connection_set()
{
    return connection_mask;
}


multi_Status multi_connect(multi_ConnectedCallback callback,
                           multi_ConnectionHostCallback host_callback,
                           multi_SendCallback send_callback,
                           multi_DataCallback data_callback)
{
    g_data_callback = data_callback;
    g_send_callback = send_callback;

    REG_RCNT = R_MULTI;
    REG_SIOCNT = SIO_MULTI;
    REG_SIOCNT |= SIO_IRQ | SIO_115200;

    multi_register_serial_isr(multi_connect_serial_isr);
    multi_enable_serial_irq(1);

    connection_mask = 0;

    if (multiplayer_is_master()) {
        while (1) {
            // When the host determines that it's time to advance to an active
            // multiplayer session, it writes a start command, and returns.
            if (host_callback()) {
                REG_SIOMLT_SEND = MULTI_DEVICE_START;
                REG_SIOCNT |= SIO_START;

                // Wait a bit for the start command to propagate.
                busy_wait(40000);

                multi_serial_init();

                return multi_Status_success;

            } else {
                // Ok, so we're going to send out a ready integer constant, and
                // see which devices ping back a ready response.
                REG_SIOMLT_SEND = MULTI_DEVICE_READY;
                REG_SIOCNT |= SIO_START;

                // FIXME... busy wait for now. We should really be waiting on a
                // timer interrupt. But I'm feeling lazy.
                busy_wait(20000);

                multi_connect_check_devices(&connection_mask, callback);
            }
        }
    } else {
        REG_SIOMLT_SEND = MULTI_DEVICE_READY;

        while (1) {
            REG_SIOMLT_SEND = MULTI_DEVICE_READY;

            while (!sio_got_intr) ; // Wait for serial interrupt.
            sio_got_intr = 0;

            // If we've received a start command from the master, now let's set
            // up the multiplayer session.
            if (REG_SIOMULTI0 == MULTI_DEVICE_START) {

                multi_serial_init();

                return multi_Status_success;
            } else {
                multi_connect_check_devices(&connection_mask, callback);
            }
        }
    }

    return multi_Status_success;
}


static void multi_master_timer_isr()
{
    multi_enable_timer2_irq(0);
    multi_tx_send();
    REG_SIOCNT |= SIO_START;
}


static void multi_serial_master_init_timer()
{
    // These times must be carefully calibrated. If you set the time too small,
    // you risk starting a transmission before the previous transmission
    // finished. I am using these same timer values in Blind Jump, an open
    // source gba game. I think pokemon uses similar values. You can try to
    // increase the frequency of the transmissions, but do so at your own risk.
    REG_TM2CNT_H = 0x00C1;
    REG_TM2CNT_L = 65000;

    multi_register_timer2_isr(multi_master_timer_isr);
    multi_enable_timer2_irq(1);
}


static void multi_schedule_master_tx()
{
    // We cannot transmit right away, we need to wait for the signal to
    // propagate through the rest of the devices. If the master transmits too
    // soon, then it will blow mess up the current transmission. So instead,
    // we're using a timer interrupt to schedule the next transmission.
    multi_serial_master_init_timer();
}


static void multi_schedule_tx()
{
    if (multiplayer_is_master()) {
        multi_schedule_master_tx();
    } else {
        multi_tx_send();
    }
}


static void multi_serial_isr()
{
    multi_record_id();


    g_data_callback(REG_SIOMULTI0,
                    REG_SIOMULTI1,
                    REG_SIOMULTI2,
                    REG_SIOMULTI3);

    multi_schedule_tx();
}


static void multi_serial_init()
{
    REG_SIOMLT_SEND = 0;

    if (multiplayer_is_master()) {
        // The master drives the whole transmission sequence. Up until this
        // point, we've been cheating a bit, by using busy-waits to schedule
        // transmissions. But we can't just sit around doing nothing while we
        // wait for data, so we're going to use a carefully calibrated timer
        // interrupt instead, to drive the transmissions.
        multi_serial_master_init_timer();
    }

    multi_register_serial_isr(multi_serial_isr);
    multi_enable_serial_irq(1);

    if (!multiplayer_is_master()) {
        REG_SIOMLT_SEND = 0;
    }

    multi_schedule_tx();
}


multi_PlayerId multi_id()
{
    return g_multi_id;
}
