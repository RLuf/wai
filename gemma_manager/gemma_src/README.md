# gemma.cpp

gemma.cpp is a lightweight, standalone C++ inference engine for the Gemma
foundation models from Google.

For additional information about Gemma, see
[ai.google.dev/gemma](https://ai.google.dev/gemma). Model weights, including
gemma.cpp specific artifacts, are
[available on kaggle](https://www.kaggle.com/models/google/gemma).

## Who is this project for?

Modern LLM inference engines are sophisticated systems, often with bespoke
capabilities extending beyond traditional neural network runtimes. With this
comes opportunities for research and innovation through co-design of high level
algorithms and low-level computation. However, there is a gap between
deployment-oriented C++ inference runtimes, which are not designed for
experimentation, and Python-centric ML research frameworks, which abstract away
low-level computation through compilation.

gemma.cpp provides a minimalist implementation of Gemma-1, Gemma-2, Gemma-3, and
PaliGemma models, focusing on simplicity and directness rather than full
generality. This is inspired by vertically-integrated model implementations such
as [ggml](https://github.com/ggerganov/ggml),
[llama.c](https://github.com/karpathy/llama2.c), and
[llama.rs](https://github.com/srush/llama2.rs).

gemma.cpp targets experimentation and research use cases. It is intended to be
straightforward to embed in other projects with minimal dependencies and also
easily modifiable with a small ~2K LoC core implementation (along with ~4K LoC
of supporting utilities). We use the [Google
Highway](https://github.com/google/highway) Library to take advantage of
portable SIMD for CPU inference.

For production-oriented edge deployments we recommend standard deployment
pathways using Python frameworks like JAX, Keras, PyTorch, and Transformers
([all model variations here](https://www.kaggle.com/models/google/gemma)).

## Contributing

Community contributions large and small are welcome. See
[DEVELOPERS.md](https://github.com/google/gemma.cpp/blob/main/DEVELOPERS.md)
for additional notes contributing developers and [join the discord by following
this invite link](https://discord.gg/H5jCBAWxAe). This project follows
[Google's Open Source Community
Guidelines](https://opensource.google.com/conduct/).

*Active development is currently done on the `dev` branch. Please open pull
requests targeting `dev` branch instead of `main`, which is intended to be more
stable.*

## Quick Start

### System requirements

Before starting, you should have installed:

- [CMake](https://cmake.org/)
- [Clang C++ compiler](https://clang.llvm.org/get_started.html), supporting at
  least C++17.
- `tar` for extracting archives from Kaggle.

Building natively on Windows requires the Visual Studio 2012 Build Tools with the
optional Clang/LLVM C++ frontend (`clang-cl`). This can be installed from the
command line with
[`winget`](https://learn.microsoft.com/en-us/windows/package-manager/winget/):

```sh
winget install --id Kitware.CMake
winget install --id Microsoft.VisualStudio.2022.BuildTools --force --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools;installRecommended --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset"
```

### Step 1: Obtain model weights and tokenizer from Kaggle or Hugging Face Hub

Visit the
[Kaggle page for Gemma-2](https://www.kaggle.com/models/google/gemma-2/gemmaCpp)
[or Gemma-1](https://www.kaggle.com/models/google/gemma/frameworks/gemmaCpp),
and select `Model Variations |> Gemma C++`.

On this tab, the `Variation` dropdown includes the options below. Note bfloat16
weights are higher fidelity, while 8-bit switched floating point weights enable
faster inference. In general, we recommend starting with the `-sfp` checkpoints.

If you are unsure which model to start with, we recommend starting with the
smallest Gemma-2 model, i.e. `2.0-2b-it-sfp`.

Alternatively, visit the
[gemma.cpp](https://huggingface.co/models?other=gemma.cpp) models on the Hugging
Face Hub. First go the model repository of the model of interest (see
recommendations below). Then, click the `Files and versions` tab and download
the model and tokenizer files. For programmatic downloading, if you have
`huggingface_hub` installed, you can also download by running:

```
huggingface-cli login # Just the first time
huggingface-cli download google/gemma-2b-sfp-cpp --local-dir build/
```

Gemma-1 2B instruction-tuned (`it`) and pre-trained (`pt`) models:

| Model name  | Description |
| ----------- | ----------- |
| `2b-it`     | 2 billion parameter instruction-tuned model, bfloat16 |
| `2b-it-sfp` | 2 billion parameter instruction-tuned model, 8-bit switched floating point |
| `2b-pt`     | 2 billion parameter pre-trained model, bfloat16 |
| `2b-pt-sfp` | 2 billion parameter pre-trained model, 8-bit switched floating point |

Gemma-1 7B instruction-tuned (`it`) and pre-trained (`pt`) models:

| Model name  | Description |
| ----------- | ----------- |
| `7b-it`     | 7 billion parameter instruction-tuned model, bfloat16 |
| `7b-it-sfp` | 7 billion parameter instruction-tuned model, 8-bit switched floating point |
| `7b-pt`     | 7 billion parameter pre-trained model, bfloat16 |
| `7b-pt-sfp` | 7 billion parameter pre-trained model, 8-bit switched floating point |

> [!NOTE]
> **Important**: We strongly recommend starting off with the `2b-it-sfp` model to
> get up and running.

Gemma 2 models are named `gemma2-2b-it` for 2B and `9b-it` or `27b-it`. See the
`kModelFlags` definition in `common.cc`.

### Step 2: Extract Files

If you downloaded the models from Hugging Face, skip to step 3.

After filling out the consent form, the download should proceed to retrieve a
tar archive file `archive.tar.gz`. Extract files from `archive.tar.gz` (this can
take a few minutes):

```
tar -xf archive.tar.gz
```

This should produce a file containing model weights such as `2b-it-sfp.sbs` and
a tokenizer file (`tokenizer.spm`). You may want to move these files to a
convenient directory location (e.g. the `build/` directory in this repo).

### Step 3: Build

The build system uses [CMake](https://cmake.org/). To build the gemma inference
runtime, create a build directory and generate the build files using `cmake`
from the top-level project directory. Note if you previous ran `cmake` and are
re-running with a different setting, be sure to delete all files in the `build/`
directory with `rm -rf build/*`.

#### Unix-like Platforms
```sh
cmake -B build
```

After running `cmake`, you can enter the `build/` directory and run `make` to
build the `./gemma` executable:

```sh
# Configure `build` directory
cmake --preset make

# Build project using make
cmake --build --preset make -j [number of parallel threads to use]
```

Replace `[number of parallel threads to use]` with a number - the number of
cores available on your system is a reasonable heuristic.  For example,
`make -j4 gemma` will build using 4 threads. If the `nproc` command is
available, you can use `make -j$(nproc) gemma` as a reasonable default
for the number of threads.

If you aren't sure of the right value for the `-j` flag, you can simply run
`make gemma` instead and it should still build the `./gemma` executable.

> [!NOTE]
> On Windows Subsystem for Linux (WSL) users should set the number of
> parallel threads to 1. Using a larger number may result in errors.

If the build is successful, you should now have a `gemma` executable in the `build/` directory.

#### Windows

```sh
# Configure `build` directory
cmake --preset windows

# Build project using Visual Studio Build Tools
cmake --build --preset windows -j [number of parallel threads to use]
```

If the build is successful, you should now have a `gemma.exe` executable in the `build/` directory.

#### Bazel

```sh
bazel build -c opt --cxxopt=-std=c++20 :gemma
```

If the build is successful, you should now have a `gemma` executable in the `bazel-bin/` directory.

#### Make

If you prefer Makefiles, @jart has made one available here:

https://github.com/jart/gemma3/blob/main/Makefile

### Step 4: Run

You can now run `gemma` from inside the `build/` directory.

`gemma` has the following required arguments:

Argument        | Description                  | Example value
--------------- | ---------------------------- | -----------------------
`--model`       | The model type.              | `2b-it` ... (see below)
`--weights`     | The compressed weights file. | `2b-it-sfp.sbs`
`--weight_type` | The compressed weight type.  | `sfp`
`--tokenizer`   | The tokenizer file.          | `tokenizer.spm`

`gemma` is invoked as:

```sh
./gemma \
--tokenizer [tokenizer file] \
--weights [compressed weights file] \
--weight_type [f32 or bf16 or sfp (default:sfp)] \
--model [2b-it or 2b-pt or 7b-it or 7b-pt or ...]
```

Example invocation for the following configuration:

- Compressed weights file `2b-it-sfp.sbs` (2B instruction-tuned model, 8-bit
  switched floating point).
- Tokenizer file `tokenizer.spm`.

```sh
./gemma \
--tokenizer tokenizer.spm \
--weights 2b-it-sfp.sbs --model 2b-it
```

### RecurrentGemma

This repository includes a version of Gemma based on Griffin
([paper](https://arxiv.org/abs/2402.19427),
[code](https://github.com/google-deepmind/recurrentgemma)). Its architecture
includes both recurrent layers and local attention, thus it is more efficient
for longer sequences and has a smaller memory footprint than standard Gemma. We
here provide a C++ implementation of this model based on the paper.

To use the recurrent version of Gemma included in this repository, build the
gemma binary as noted above in Step 3. Download the compressed weights and
tokenizer from the RecurrentGemma
[Kaggle](https://www.kaggle.com/models/google/recurrentgemma/gemmaCpp) as in
Step 1, and run the binary as follows:

`./gemma --tokenizer tokenizer.spm --model gr2b-it --weights 2b-it-sfp.sbs`

### PaliGemma Vision-Language Model

This repository includes a version of the PaliGemma VLM
([paper](https://arxiv.org/abs/2407.07726),
[code](https://github.com/google-research/big_vision/tree/main/big_vision/configs/proj/paligemma))
and its successor PaliGemma 2 ([paper](https://arxiv.org/abs/2412.03555)). We
provide a C++ implementation of the PaliGemma model family here.

To use the version of PaliGemma included in this repository, build the gemma
binary as noted above in Step 3. Download the compressed weights and tokenizer
from
[Kaggle](https://www.kaggle.com/models/google/paligemma/gemmaCpp/paligemma-3b-mix-224)
and run the binary as follows:

```sh
./gemma \
--tokenizer paligemma_tokenizer.model \
--model paligemma-224 \
--weights paligemma-3b-mix-224-sfp.sbs \
--image_file paligemma/testdata/image.ppm
```

Note that the image reading code is very basic to avoid depending on an image
processing library for now. We currently only support reading binary PPMs (P6).
So use a tool like `convert` to first convert your images into that format, e.g.

`convert image.jpeg -resize 224x224^ image.ppm`

(As the image will be resized for processing anyway, we can already resize at
this stage for slightly faster loading.)

The interaction with the image (using the mix-224 checkpoint) may then look
something like this:

```
> Describe the image briefly
A large building with two towers in the middle of a city.
> What type of building is it?
church
> What color is the church?
gray
> caption image
A large building with two towers stands tall on the water's edge. The building
has a brown roof and a window on the side. A tree stands in front of the
building, and a flag waves proudly from its top. The water is calm and blue,
reflecting the sky above. A bridge crosses the water, and a red and white boat
rests on its surface. The building has a window on the side, and a flag on top.
A tall tree stands in front of the building, and a window on the building is
visible from the water. The water is green, and the sky is blue.
```

### Migrating to single-file format

There is now a new format for the weights file, which is a single file that
allows to contain the tokenizer (and the model type) directly. A tool to migrate
from the multi-file format to the single-file format is available.

```sh
compression/migrate_weights \
  --tokenizer .../tokenizer.spm --weights .../gemma2-2b-it-sfp.sbs \
  --model gemma2-2b-it --output_weights .../gemma2-2b-it-sfp-single.sbs
```

After migration, you can use the new weights file with gemma.cpp like this:

```sh
./gemma --weights .../gemma2-2b-it-sfp-single.sbs
```

### Troubleshooting and FAQs

**Running `./gemma` fails with "Failed to read cache gating_ein_0 (error 294) ..."**

The most common problem is that the `--weight_type` argument does not match that
of the model file. Revisit step #3 and check which weights you downloaded.

Note that we have already moved weight type from a compile-time decision to a
runtime argument. In a subsequent step, we plan to bake this information into
the weights.

**Problems building in Windows / Visual Studio**

Currently if you're using Windows, we recommend building in WSL (Windows
Subsystem for Linux). We are exploring options to enable other build
configurations, see issues for active discussion.

**Model does not respond to instructions and produces strange output**

A common issue is that you are using a pre-trained model, which is not
instruction-tuned and thus does not respond to instructions. Make sure you are
using an instruction-tuned model (`2b-it-sfp`, `2b-it`, `7b-it-sfp`, `7b-it`)
and not a pre-trained model (any model with a `-pt` suffix).

**What sequence lengths are supported?**

See `seq_len` in `configs.cc`. For the Gemma 3 models larger than 1B, this is
typically 32K but 128K would also work given enough RAM. Note that long
sequences will be slow due to the quadratic cost of attention.

**How do I convert my fine-tune to a `.sbs` compressed model file?**

For PaliGemma (1 and 2) checkpoints, you can use
python/convert_from_safetensors.py to convert from safetensors format (tested
with building via bazel). For an adapter model, you will likely need to call
merge_and_unload() to convert the adapter model to a single-file format before
converting it.

Here is how to use it using a bazel build of the compression library assuming
locally installed (venv) torch, numpy, safetensors, absl-py, etc.:

```sh
bazel build //compression/python:compression
BAZEL_OUTPUT_DIR="${PWD}/bazel-bin/compression"
python3 -c "import site; print(site.getsitepackages())"
# Use your sites-packages file here:
ln -s $BAZEL_OUTPUT_DIR [...]/site-packages/compression
python3 python/convert_from_safetensors.py --load_path [...].safetensors.index.json
```

See also compression/convert_weights.py for a slightly older option to convert a
pytorch checkpoint. (The code may need updates to work with Gemma-2 models.)

**What are some easy ways to make the model run faster?**

1.  Make sure you are using the 8-bit switched floating point `-sfp` models.
    These are half the size of bf16 and thus use less memory bandwidth and cache
    space.
2.  If you're on a laptop, make sure power mode is set to maximize performance
    and saving mode is **off**. For most laptops, the power saving modes get
    activated automatically if the computer is not plugged in.
3.  Close other unused cpu-intensive applications.
4.  On macs, anecdotally we observe a "warm-up" ramp-up in speed as performance
    cores get engaged.
5.  Experiment with the `--num_threads` argument value. Depending on the device,
    larger numbers don't always mean better performance.

We're also working on algorithmic and optimization approaches for faster
inference, stay tuned.

## Usage

`gemma` has different usage modes, controlled by the verbosity flag.

All usage modes are currently interactive, triggering text generation upon
newline input.

| Verbosity       | Usage mode | Details                                       |
| --------------- | ---------- | --------------------------------------------- |
| `--verbosity 0` | Minimal | Only prints generation output. Suitable as a CLI tool. |
| `--verbosity 1` | Default | Standard user-facing terminal UI. |
| `--verbosity 2` | Detailed | Shows additional developer and debug info. |

### Interactive Terminal App

By default, verbosity is set to 1, bringing up a terminal-based interactive
interface when `gemma` is invoked:

```console
$ ./gemma [...]
  __ _  ___ _ __ ___  _ __ ___   __ _   ___ _ __  _ __
 / _` |/ _ \ '_ ` _ \| '_ ` _ \ / _` | / __| '_ \| '_ \
| (_| |  __/ | | | | | | | | | | (_| || (__| |_) | |_) |
 \__, |\___|_| |_| |_|_| |_| |_|\__,_(_)___| .__/| .__/
  __/ |                                    | |   | |
 |___/                                     |_|   |_|

tokenizer                     : tokenizer.spm
compressed_weights            : 2b-it-sfp.sbs
model                         : 2b-it
weights                       : [no path specified]
max_generated_tokens          : 2048

*Usage*
  Enter an instruction and press enter (%C reset conversation, %Q quits).

*Examples*
  - Write an email to grandma thanking her for the cookies.
  - What are some historical attractions to visit around Massachusetts?
  - Compute the nth fibonacci number in javascript.
  - Write a standup comedy bit about WebGPU programming.

> What are some outdoorsy places to visit around Boston?

[ Reading prompt ] .....................


**Boston Harbor and Islands:**

* **Boston Harbor Islands National and State Park:** Explore pristine beaches, wildlife, and maritime history.
* **Charles River Esplanade:** Enjoy scenic views of the harbor and city skyline.
* **Boston Harbor Cruise Company:** Take a relaxing harbor cruise and admire the city from a different perspective.
* **Seaport Village:** Visit a charming waterfront area with shops, restaurants, and a seaport museum.

**Forest and Nature:**

* **Forest Park:** Hike through a scenic forest with diverse wildlife.
* **Quabbin Reservoir:** Enjoy boating, fishing, and hiking in a scenic setting.
* **Mount Forest:** Explore a mountain with breathtaking views of the city and surrounding landscape.

...
```

### Usage as a Command Line Tool

For using the `gemma` executable as a command line tool, it may be useful to
create an alias for gemma.cpp with arguments fully specified:

```sh
alias gemma2b="~/gemma.cpp/build/gemma -- --tokenizer ~/gemma.cpp/build/tokenizer.spm --weights ~/gemma.cpp/build/gemma2-2b-it-sfp.sbs --model gemma2-2b-it --verbosity 0"
```

Replace the above paths with your own paths to the model and tokenizer paths
from the download.

Here is an example of prompting `gemma` with a truncated input
file (using a `gemma2b` alias like defined above):

```sh
cat configs.h | tail -n 35 | tr '\n' ' ' | xargs -0 echo "What does this C++ code do: " | gemma2b
```

> [!NOTE]
> CLI usage of gemma.cpp is experimental and should take context length
> limitations into account.

The output of the above command should look like:

```console
[ Reading prompt ] [...]
This C++ code snippet defines a set of **constants** used in a large language model (LLM) implementation, likely related to the **attention mechanism**.

Let's break down the code:
[...]
```

### Incorporating gemma.cpp as a Library in your Project

The easiest way to incorporate gemma.cpp in your own project is to pull in
gemma.cpp and dependencies using `FetchContent`. You can add the following to your
CMakeLists.txt:

```
include(FetchContent)

FetchContent_Declare(sentencepiece GIT_REPOSITORY https://github.com/google/sentencepiece GIT_TAG 53de76561cfc149d3c01037f0595669ad32a5e7c)
FetchContent_MakeAvailable(sentencepiece)

FetchContent_Declare(gemma GIT_REPOSITORY https://github.com/google/gemma.cpp GIT_TAG origin/main)
FetchContent_MakeAvailable(gemma)

FetchContent_Declare(highway GIT_REPOSITORY https://github.com/google/highway.git GIT_TAG da250571a45826b21eebbddc1e50d0c1137dee5f)
FetchContent_MakeAvailable(highway)
```

Note for the gemma.cpp `GIT_TAG`, you may replace `origin/main` for a specific
commit hash if you would like to pin the library version.

After your executable is defined (substitute your executable name for
`[Executable Name]` below):

```
target_link_libraries([Executable Name] libgemma hwy hwy_contrib sentencepiece)
FetchContent_GetProperties(gemma)
FetchContent_GetProperties(sentencepiece)
target_include_directories([Executable Name] PRIVATE ${gemma_SOURCE_DIR})
target_include_directories([Executable Name] PRIVATE ${sentencepiece_SOURCE_DIR})
```

### Building gemma.cpp as a Library

gemma.cpp can also be used as a library dependency in your own project. The
shared library artifact can be built by modifying the make invocation to build
the `libgemma` target instead of `gemma`.

> [!NOTE]
> If you are using gemma.cpp in your own project with the `FetchContent` steps
> in the previous section, building the library is done automatically by `cmake`
> and this section can be skipped.

First, run `cmake`:

```sh
cmake -B build
```

Then, run `make` with the `libgemma` target:

```sh
cd build
make -j [number of parallel threads to use] libgemma
```

If this is successful, you should now have a `libgemma` library file in the
`build/` directory. On Unix platforms, the filename is `libgemma.a`.

## Independent Projects Using gemma.cpp

Some independent projects using gemma.cpp:

- [gemma-cpp-python - Python bindings](https://github.com/namtranase/gemma-cpp-python)
- [lua-cgemma - Lua bindings](https://github.com/ufownl/lua-cgemma)
- [Godot engine demo project](https://github.com/Rliop913/Gemma-godot-demo-project)

If you would like to have your project included, feel free to get in touch or
submit a PR with a `README.md` edit.

## Acknowledgements and Contacts

gemma.cpp was started in fall 2023 by [Austin Huang](mailto:austinvhuang@google.com)
and [Jan Wassenberg](mailto:janwas@google.com), and subsequently released February 2024
thanks to contributions from Phil Culliton, Paul Chang, and Dan Zheng.

Griffin support was implemented in April 2024 thanks to contributions by Andrey
Mikhaylov, Eugene Kliuchnikov, Jan Wassenberg, Jyrki Alakuijala, Lode
Vandevenne, Luca Versari, Martin Bruse, Phil Culliton, Sami Boukortt, Thomas
Fischbacher and Zoltan Szabadka.

Gemma-2 support was implemented in June/July 2024 with the help of several
people.

PaliGemma support was implemented in September 2024 with contributions from
Daniel Keysers.

[Jan Wassenberg](mailto:janwas@google.com) has continued to contribute many
improvements, including major gains in efficiency, since the initial release.

This is not an officially supported Google product.
