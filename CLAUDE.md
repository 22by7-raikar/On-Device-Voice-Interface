# Working in this repository

On-device voice interface for robots: a robot that hears a person across a
noisy room, turns toward them, understands a free-form request, acts on it
safely, and stops the instant it is told to. C++17, whisper.cpp, an ASUS ROG
Zephyrus G14 (2021, RTX 3060 6 GB, Ubuntu) as the development and demo
machine. Read `README.md`, `docs/architecture.md` and `docs/decisions.md`
before changing anything.

## The bar

Everything here (code, tests, reviews, documentation, explanations) is held
to a staff/principal engineering standard. In practice:

- **Every component earns its place** by what the robot needs (see the table
  in the README). If a change cannot be justified that way, it does not go in.
- **Numbers are measured, never estimated**, and always reported with their
  setup: data, hardware, build type, model, settings. Unknown is written as
  unknown.
- **Tests fail without the change.** A test that would pass on the old code
  proves nothing. Prefer tests that check behaviour against an independent
  reference (an ideal signal, an analytic result) over tests that restate the
  implementation.
- **Contracts are enforced in every build type.** Documented preconditions
  throw `std::invalid_argument`; never rely on `assert` alone.
- **Decisions with lasting consequences go in `docs/decisions.md`** (context,
  decision, consequences), written when they are made.
- **Docs move with the code.** README, architecture doc, Doxygen comments and
  diagrams describe what the code does now.
- **Say what was not verified.** Every PR has a "Not verified" section.
- **Plain, precise language.** No marketing words, no vague claims.
- **Review your own diff** before opening a PR, as a demanding reviewer would.

## Workflow

- Never push to `main`. Branch, then open a PR using
  `.github/pull_request_template.md`. The owner merges.
- Commits are authored as the repository owner. Do not add `Co-Authored-By`
  or any other tool attribution to commits or PRs.
- Commit messages explain why, not only what.
- Before a PR: `release` and `asan` suites pass, clang-format is clean, and
  `doxygen docs/Doxyfile` has no warnings (CI enforces all four).
- Keep PRs to one concern. A milestone is usually several PRs.

## Build and test

```bash
git submodule update --init
cmake --preset release && cmake --build --preset release -j && ctest --preset release
cmake --preset asan && cmake --build --preset asan -j && ctest --preset asan
cmake --preset cuda && cmake --build --preset cuda -j        # GPU build
doxygen docs/Doxyfile                                       # API docs, warnings are errors
docs/diagrams/render.sh                                     # Mermaid -> SVG (needs mmdc)
scripts/download_model.sh base.en                           # whisper model into models/
scripts/listen.sh --model models/ggml-base.en.bin --mic     # live test
```

## Architecture rules

- `odvi_core` never links a model runtime; it reaches ASR only through the
  `Transcriber` callable. Tests link `odvi_core` only and need no weights.
- `odvi_asr` is the only target that links whisper.cpp.
- `apps/` contain argument parsing and wiring, no algorithms.
- Everything after ingress runs at 16 kHz (`kInternalSampleRate`); input is
  converted once by the anti-aliased `Resampler` (decision D1). Multichannel
  front-end stages will run before that conversion, at the capture rate.
- Each utterance produces exactly one JSON event, including failures; ASR runs
  exactly once per utterance (D3).
- llama.cpp is added in M4, with a single shared ggml (D4).
- No model training (D5). Public datasets are used for evaluation only.
- Out of scope: ROS, a web UI, cloud services, end-to-end speech-to-speech
  models (a design note comparing them is fine).

## Where things stand

- **M0 done:** listening loop, polyphase resampler, CI, docs.
- **CUDA build fails.** It has never compiled successfully. CI runs it weekly
  and on demand only (PR #2, if not yet merged). First task on this machine:
  `cmake --preset cuda && cmake --build --preset cuda`, fix the error, and
  confirm `odvi_listen` reports `backend=cuda`.
- **Real transcription has not been run yet.** Verify with
  `scripts/listen.sh` on the G14 and record what was checked.

## Next: M1, benchmark harness and VAD comparison

Write the evaluation protocol down (in `docs/evaluation.md`) **before**
producing results, so the numbers cannot be tuned to the test set.

- **Data** (on this laptop; paths to be recorded in the protocol, data never
  committed): LibriSpeech test-clean and test-other; MUSAN (music, noise,
  babble); DEMAND; a room impulse response set. Commit manifests (file lists,
  seeds, mixing parameters), not audio.
- **Mixtures:** speech + noise at 20, 10, 5, 0 and −5 dB SNR, seeded and
  reproducible; music-only and noise-only clips for false triggers; G14 fan
  noise recorded under GPU load as real ego-noise.
- **ASR:** WER vs SNR for whisper tiny.en, base.en and small.en, with a fixed,
  documented text normaliser; real-time factor on CPU and CUDA.
- **VAD comparison:** the energy/ZCR baseline in this repo; the learned
  quality gate from `cpp-edge-audio`; Silero VAD (ONNX); Whisper's no-speech
  probability. Report frame-level precision/recall, utterance-level false
  triggers per hour on music and noise, and missed-speech rate.
- **Output:** results tables and plots committed under `docs/results/`, each
  with the command that regenerates it.

Later milestones (M2 front end, M3 real-time loop, M4 agent, M5 TTS/echo
cancellation/barge-in, M6 evaluation and demo) are in the README roadmap.

## Hardware notes

- G14 built-in microphones: believed to be a 2-channel array, not yet
  verified. Check with `arecord -l` and a 5 s stereo recording (speak from the
  left, then the right) and measure the mic spacing; at 5 cm, spatial aliasing
  starts near 3.4 kHz. If only one channel is exposed, M2 uses simulated arrays
  (pyroomacoustics) plus a ReSpeaker USB 4-mic array.
- 6 GB VRAM budget: whisper small (~1 GB) plus a 3B Q4 LLM (~2.5 GB); VAD,
  denoising and TTS stay on the CPU.

## Related repositories

- `cpp-edge-audio`: where the audio decoding, segmentation and ASR code came
  from, and where the learned quality gate lives (an M1 baseline).
- `marine-ptz-tracker`: a later, separate follow-on reuses the direction of
  arrival code for audio-cued camera tracking. Not part of this project's
  milestones.
