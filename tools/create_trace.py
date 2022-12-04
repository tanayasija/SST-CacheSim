#!/bin/python3
import argparse
import re

parser = argparse.ArgumentParser()
parser.add_argument("--trace", help = "Complete path to trace file to be analyzed", required=True)
parser.add_argument("--num-threads", help = "Number of thread in trace", required=True)
parser.add_argument("--out-dir", help = "Output directory to store processed trace files", required=True)
parser.add_argument("--name", help = "Name to be assigned to parsed trace files", default="trace")

args = parser.parse_args()
trace_path = args.trace
trace_name = args.name.strip()
num_threads = int(args.num_threads)
out_dir = args.out_dir

trace_files = []
reexp = []
for i in range(1, num_threads + 1):
    f = open(f"{out_dir}/{trace_name}_{str(i-1)}.txt", "w")
    trace_files.append(f)
    reexp.append(re.compile(f"threadId: {str(i)}"))

trace_file = open(trace_path)
trace = trace_file.read().strip().split("\n")
trace_file.close()

for line in trace:
    # print(line)
    for i in range(1, num_threads + 1):
        if reexp[i-1].match(line):
            trace_files[i-1].write(f"{line}\n")


