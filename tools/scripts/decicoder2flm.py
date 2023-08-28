import sys
from transformers import AutoModelForCausalLM, AutoTokenizer
from fastllm_pytools import torch2flm

if __name__ == "__main__":
    checkpoint = "Deci/DeciCoder-1b"
    device = "cpu"

    tokenizer = AutoTokenizer.from_pretrained(checkpoint)
    model = AutoModelForCausalLM.from_pretrained(checkpoint, trust_remote_code=True).to(device)

    dtype = sys.argv[2] if len(sys.argv) >= 3 else "float16"
    exportPath = sys.argv[1] if len(sys.argv) >= 2 else "decicoder-" + dtype + ".flm"
    torch2flm.tofile(exportPath, model, tokenizer, dtype = dtype)