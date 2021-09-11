#pragma once


typedef enum {
    multi_Status_success,
    multi_Status_failure
} multi_Status;


typedef enum {
    multi_PlayerId_host    = (1 << 0),
    multi_PlayerId_p1      = (1 << 1),
    multi_PlayerId_p2      = (1 << 2),
    multi_PlayerId_p3      = (1 << 3),
    multi_PlayerId_unknown = (1 << 4),
} multi_PlayerId;


// Return subset of connected devices, recorded at the time when multi_connect
// was called. If you want to determine the status of other devices, you'll need
// to do it yourself, becaues there's no way to check via the hardware which
// devices are connected.
multi_PlayerId multi_connection_set();


typedef void (*multi_DataCallback)(unsigned short host_data,
                                   unsigned short p1_data,
                                   unsigned short p2_data,
                                   unsigned short p3_data,
                                   volatile unsigned short* output);


typedef void (*multi_ConnectedCallback)(multi_PlayerId id, int connected);
typedef int (*multi_ConnectionHostCallback)();


// A Blocking call, but invokes a callback as each other player
// connects. multi_connect will call the `device_connected` callback for each
// connected gba device.
//
// `host_init_callback` will be invoked on the host device. Upon returning true
// from the host callback, the multiplayer session is now active, and all gba's
// that were registered are now associated with with the multiplayer session.
multi_Status multi_connect(multi_ConnectedCallback device_connected,
                           multi_ConnectionHostCallback host_init_callback,
                           multi_DataCallback data_callback);


// Return our own id.
multi_PlayerId multi_id();
