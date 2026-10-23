# Decision log

Short records of decisions that shape the code, written when they are made.
Each states the context, the decision, and what it costs. A decision is
changed by adding a new entry that supersedes it, not by editing history.

## D1. One internal sample rate, converted once at ingress

*Status: accepted (M0).*

**Context.** Microphones deliver 44.1 or 48 kHz; whisper.cpp accepts only
16 kHz. The first version ran the VAD at the input rate and resampled each
utterance with linear interpolation just before ASR. Linear interpolation has
no anti-aliasing filter: measured on a 48 kHz → 16 kHz conversion, tones at
8.5–23 kHz fold into the 0–8 kHz band at full level, including into the speech
band ASR depends on. Fan and motor noise has energy across this range.

**Decision.** Everything after ingress runs at 16 kHz. `ListenLoop` converts
the stream once with a Kaiser-windowed sinc polyphase `Resampler` (≥ 80 dB
stopband from the output Nyquist, passband to 6.8 kHz) before the VAD.

**Consequences.** The VAD, segmenter, ASR and every later single-channel stage
see the same band-limited signal, and their parameters (frame sizes, memory
bounds) are defined once. Ingress adds about 2 ms of look-ahead latency at
48 kHz. Multichannel front-end stages (M2) will run before this conversion, at
the capture rate, and are not covered by this decision.

## D2. Synchronous listening loop first

*Status: accepted (M0); revisited in M3.*

**Context.** ASR on a whole utterance takes tens to hundreds of milliseconds.
Running it inside `push()` stalls input reading for that long.

**Decision.** Keep the loop single-threaded until there is a measurement to
improve. The OS pipe buffers audio during a decode, so nothing is lost at
current utterance lengths.

**Consequences.** Deterministic, byte-for-byte replayable behaviour, which the
tests rely on. M3 moves ASR to a worker thread and must show the latency
change against this baseline.

## D3. ASR exactly once per utterance

*Status: accepted (M0).*

**Context.** Transcribing fixed windows cuts words at window edges and makes
cost depend on window size rather than on what was said.

**Decision.** The segmenter decides utterance boundaries; ASR runs once on the
whole utterance, capped at 8 s.

**Consequences.** Predictable cost and no split words. Latency includes the
600 ms end-of-speech wait, which M3 measures and tunes.

## D4. Defer llama.cpp until the agent milestone

*Status: accepted (M0).*

**Context.** whisper.cpp and llama.cpp each vendor their own copy of ggml.
Adding both as CMake subprojects defines the `ggml` targets twice.

**Decision.** Add llama.cpp in M4, when it is first used, and resolve the
shared ggml then (one ggml build both link against, or pinned versions that
share a ggml revision).

**Consequences.** No unused dependency in the tree; the integration risk is
named and scheduled rather than discovered.

## D5. No model training

*Status: accepted (project scope).*

**Context.** The project is about systems, signal processing and evaluation on
device, not about training models.

**Decision.** Use pretrained open checkpoints only. Public datasets
(LibriSpeech, MUSAN, DEMAND, RIRs) are used for evaluation, never training.

**Consequences.** Results measure integration and front-end quality, and any
improvement must come from signal processing, configuration or system design.
