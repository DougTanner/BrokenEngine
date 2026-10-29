## Scope and authorization

User explicitly authorized manually updating the developer baseline SAVE FILE header.
Worktree HEAD: a49b61276f424515ff80f746c58ac89f4e73f520.
Session baseline: cd6d410c064a5a5f905bf68efaf28493ab6fc33a.
No source, runtime compatibility code, ClientGuid files, or screenshots changed.
The save is tracked, contrary to the brief's ignored-file description; backup and evidence are ignored.
All three persist in this session worktree's Temp tree; the authorized save edit is uncommitted.

## Source version and layout proof

Frame.cpp:106 computes base144 plus engine and game collection versions.
FrameBase.h:158-161,266-269,313 includes navigation and both server collection tuples.
NavBuild.h:15 gives16; engine Explosions interpolate/post gives3/2; Pushers gives6/2.
FrameCollections.h:14-25 orders Blasters, Missiles, Fighters, Omnis, Battleships.
Their interpolate versions are3,3,7,16,3; post versions are4,13,9,22,3.
Current full sum:144+16+3+6+2+2+3+3+7+16+3+4+13+9+22+3=256.
FileManager.h:270-283 writes/reads first Int64 version, then Int64 size.
GridSave.cpp:77 validates against game::Frame::kiVersion before grid records.
git show64d29c2e Pushers.h changes5->6 and comments only; no member layout change.
CollectionLifecycle.h changes tuple-index lookup only, no persisted representation.
git diff bc69eea5 HEAD Pushers.h changes4->6 and comments only.
git diff --stat bc69eea5 HEAD Engine/Source/Frame Projects/BrokenEngineSandbox/Source/Frame
shows behavior/render/comment changes and client-only IslandTerrain residency removal.
git diff bc69eea5 HEAD Engine/Source/File/GridSave.cpp Projects/BrokenEngineSandbox/Source/Save
is empty. The previous diagnosis's layout proof remains valid through version256.

## Recorded byte verification

Save: Temp/lighting-strip-baseline/AppData/Broken Engine Sandbox Server/lighting-strip-baseline-t400
Backup: same exact path with sibling suffix .version254.bak.
Before/backup SHA256: DDC58C961B2EF2F396F4CAA614248E55BF005D6D8D1B471C02D4FCB5E42D6EDA
After SHA256: 42DD5DA00A924D39B95D985ACD09D9F104972BBD0946DBEF2FFCB1154E0AAF55
Before and after length:2547bytes. Before version254; readback version256.
Second Int64 header remains0. Bytes8:end compared individually and remain identical.
Changed byte offsets (zero-based):0,1. Header FE00000000000000 ->0001000000000000.
Backup created before opening save for write and reread to verify original identity.
Only eight version bytes were written through a FileStream; no body conversion occurred.
AlreadyUpdated=false on this execution; existing backups are verified before reuse.
Runtime loading remains for the next /agent-harness dispatch; none attempted here.
