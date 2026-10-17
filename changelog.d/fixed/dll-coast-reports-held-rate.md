- **A coasting `Dll` reports the rate it runs at.** `code_rate` kept the
    last steer's value through a coast while the NCO ran the held filter, so
    `Dll.code_rate`, the `.rate` probe and the receivers' and pool's status
    read a rate the loop was not running.
