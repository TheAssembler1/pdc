#!/bin/bash
#flux: --nodes=1
#flux: --exclusive
#flux: --time-limit=6h
#flux: --queue=pbatch
#flux: --job-name=pdc-ckpt-bench
#flux: --output=pdc-ckpt-bench-{{id}}.out
#flux: --error=pdc-ckpt-bench-{{id}}.err
#
# Batch wrapper for the PR #305 checkpoint/restart benchmark on Tuolumne.
# Submit from the repo root (the job starts in the submit directory):
#
#   flux batch ./tuolumne_ckpt_bench.flux
#   flux batch --setattr=bank=<bank> ./tuolumne_ckpt_bench.flux       # pick a bank
#   flux batch -q pdebug -t 1h --env=TRIALS=1 ./tuolumne_ckpt_bench.flux   # quick smoke run
#
# Any knob from tuolumne_ckpt_bench.sh can be passed with --env=NAME=value
# (flux batch also copies your current environment by default).
set -uo pipefail
cd "${PDC_BENCH_SRC:-$PWD}"
source ./tuolumne_env.sh
./tuolumne_ckpt_bench.sh
