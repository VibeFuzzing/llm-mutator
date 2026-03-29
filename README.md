# AFL++ Seed Mutator

Fine-tuned Llama 3.1 8B model for generating AFL++ seed mutations targeting nginx.

## Setup

1. Install [Ollama](https://ollama.com)
2. Clone this repo: `git clone https://github.com/VibeFuzzing/llm-mutator.git`
3. `cd llm-mutator`
4. `ollama create afl-mutator -f Modelfile`

## Usage

`ollama run afl-mutator`

Then provide an ancestry chain of seeds, example below:
```
[id:0 depth:1 bitmap:406 favored:False new_cov:True]
GET / HTTP/1.1\r\n\r\n
---
[id:1 depth:2 bitmap:435 favored:True new_cov:True]
GET / HTTP/1.1\r
```

The model will output a mutated seed that can be fed to AFL++.
