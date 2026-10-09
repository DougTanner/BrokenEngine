# Opus Music Compression

Revisit When: shipped data size or download size starts to matter (the first external distribution), or music grows to dominate the packed data.

## Context

`ExportAudio::Export` (`DataPacker/Source/ExportJobs/ExportAudio.cpp`) decodes each `.wav` to float, runs `audiorepair::RepairAudio`, resamples to `audiorepair::kiAudioExportSampleRate` (48 kHz), and stores raw interleaved 16-bit PCM with a `WAVE_FORMAT_PCM` `AudioHeader` (`Common/DataFile.h`). Music and sound effects share this path; a `Music` path component only limits repair.

At runtime the Audio pack is lazy and client-only (`IsEagerChunk`, `IsServerChunk` in `Engine/Source/File/PackChunks.cpp`). `StreamingVoice` (`Engine/Source/Audio/StreamingVoice.h`) plays music from start to end. It reads ahead through three 16 KiB slots (`kiBufferCount`, `kiBufferSize`) with `PackChunks::TryReadChunkData`. File loader threads do the reads, and `AudioManager::Update` on the main thread polls them and submits slots to XAudio2 in order. `GetRemainingTime` divides the remaining payload bytes by `nAvgBytesPerSec`, and `ShouldTransition` uses that time to start the `kCrossfadeDuration` crossfade to the next track. Music has no seek and no loop. `_loop` looping applies only to static voices.

Sizes, measured from the primary checkout's built `Audio.pack` (DataPacker output of 2026-10-06; this worktree has no built packs):

- The eight music chunks total 190,067,488 bytes, 98% of the 193,085,456-byte `Audio.pack` and about 12% of all packed output. `Texture.pack` alone is 1.16 GB.
- The source WAVs in `Projects/BrokenEngineSandbox/Data/Audio/Music` total 194,194,371 bytes (about 185 MiB). They are 44.1 kHz stereo: seven are 16-bit PCM and one is 32-bit float.
- 48 kHz 16-bit stereo PCM runs at 1536 kbps, so the music is about 990 s long. At about 96 kbps, Opus is about 16x smaller, roughly 12 MB. This is arithmetic, not a measured encode.

## Design

- **Scope.** Only music is encoded as Opus. Sound effects keep the current PCM path and resident playback. The existing `bMusic` test in `ExportAudio::Export` selects the encoder after repair and resampling. Opus runs natively at 48 kHz, so the mastering-rate contract in `Engine/Source/Audio/AGENTS.md` still holds.
- **Encoder (choice).** One option is to link libopusenc into DataPacker as a new ThirdParty library. The other is to run the opus-tools `opusenc` CLI, the way `ExportShader.cpp` runs `glslc`/`glslangValidator`. Unlike those tools, `opusenc` does not ship with the Vulkan SDK, so it would become a new build-machine dependency, and the repaired, resampled samples would first be written to a temporary WAV. The bitrate is also a choice; 96 kbps is the working figure.
- **Payload framing (choice).** The payload could be Ogg Opus decoded with opusfile, which also pulls in libogg. Or it could be raw Opus packets with an engine-defined length prefix decoded with libopus alone. Today's reads are sequential, so either suits the range-read path.
- **Decoder library (choice).** One option is libopus with opusfile, both BSD-3-Clause from Xiph; libopus's `COPYING` also references royalty-free patent licenses. The alternative is Vorbis through `ThirdParty/stb/stb_vorbis.c` (MIT or public domain). That file is already present but no project compiles it. It only decodes, so DataPacker would still need a Vorbis encoder library, and Vorbis gives lower quality per bit (Opus 3.999 vs Vorbis 3.513 in the HydrogenAudio 64 kbps listening test). Either way, a new library needs explicit user approval and license review under `ThirdParty/AGENTS.md`. Integration goes through `ThirdParty/Prebuilts/Source/` wrappers, and ThirdParty is otherwise not modified.
- **Pack format.** `AudioHeader` must mark the codec. It also needs the decoded length, because `GetRemainingTime` can no longer derive time from compressed bytes. Pack open rejects audio chunks that carry `kLz4Compressed` or `kZlibCompressed`, so those flags cannot mark the codec. Bump `DataHeader::kiVersion` and `ExportAudio::kiVersion` so older packs are rejected. Music has no PCM fallback.
- **Runtime decode.** `StreamingVoice` keeps its ordered range reads and slot pipeline, and also feeds the compressed bytes through a per-voice decoder into PCM slots. A crossfade therefore runs two decoders at once. PCM handed to XAudio2 still passes `AssertValidPackedAudio`. Which thread decodes is a choice:
  - the main thread inside `AudioManager::Update`, where reads are polled and slots submitted today;
  - File loader threads, which today only move bytes and would then need to know about the codec.

  XAudio2 callbacks are excluded because they publish only atomic completion.
- **Preserved behavior.** Start-to-end playback, the crossfade timing, request and submit order, retry on a full File result pool, and cancelling pending reads before a voice is destroyed or the audio graph is reset (`Engine/Source/Audio/AGENTS.md` `## Runtime Contracts`).
- **Acceptance.** The music chunks shrink by roughly the bitrate ratio, and music playback and crossfades sound and behave as they do today. Sound-effect chunks are unchanged apart from the version bump.

## Out of scope

- Compressing sound effects or any other pack type.
- Adding seek or loop to music, which has neither today.
- Changing the 48 kHz export and mastering rate.

## Notes

- Audio is client-only presentation and the server never opens the Audio pack, so this has no determinism or CRC exposure.
- `Documents/Features/Audio/ReplaceDirectXTKAudioWithMiniaudio.md` rewrites the same `StreamingVoice` code. Whichever lands second absorbs the other.
- The Debug-only `AudioStreamingHarnessRig` inspects streaming slot state, so a reshaped slot pipeline must keep it working.
- References: libopus `COPYING` https://raw.githubusercontent.com/xiph/opus/main/COPYING ; opusfile https://github.com/xiph/opusfile ; stb https://github.com/nothings/stb ; HydrogenAudio 64 kbps test https://listening-tests.hydrogenaud.io/igorc/results.html
