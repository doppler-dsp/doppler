- **The CCSDS 415.1-B-1 point is installed: `doppler/ccsds/ccsds_415.h`,
    `Ccsds415Receiver`, `Ccsds415Pool`.** Its numbers, chip rate, C/N0 and a
    scene's derived rates, computed with fast math off so every compiler
    gives the same bits; the views are the receiver and pool built there,
    refusing a code that is not 1,023 chips. The spec demo, the harnesses
    and validators read it. The scene's Python face waits on
    [just-makeit#2755](https://github.com/just-buildit/just-makeit/issues/2755).
