# ms0515-fpga

The Elektronika MS 0515 rebuilt in an FPGA down to its circuits: the
system module НС4 (`3.858.420 Э3`) as a netlist of its chips, wired as the
schematic and the board wire them, tested against the emulator
(https://github.com/vvv104/ms0515).

- `docs/sources.md` - the documents, scans and threads it is read from
- `docs/plan.md` - the stages
- `docs/findings.md` - where the sources disagree
- `CLAUDE.md` - the rules of the repository

## The ready files

The releases (https://github.com/vvv104/ms0515-fpga/releases) carry the
MiSTer core (`ms0515.rbf`, for a DE10-Nano) and both printed boards'
Gerbers with their assembly drawings (НС4 and the RAM-disk module);
`docs/release.md` says what to do with each and how to make them again.
