#!/bin/bash

# This version of the test runner doesn't attempt to run any parallel tests.
# We assume too, that if the library build has enabled MPI, that LD_LIBRARY_PATH is
# defined and points to the MPI libraries used by the linker (e.g. -L<path> -lmpi).
#
# Cori/Perlmutter CI needs srun even for serial tests.

# Select run command based on system
run_cmd=""
if [[ "$SUPERCOMPUTER" == "perlmutter" ]]; then
    run_cmd="srun -n 1 --mem=25600 --cpu_bind=cores --overlap"
fi

# Require at least one argument: the test executable
if [ $# -lt 1 ]; then
    echo "Error: Missing test executable argument" >&2
    exit 1
fi

# Check the test to be run
test_exe="$1"
shift
test_args="$@"

if [ -x "$test_exe" ]; then
    echo "Testing: $test_exe"
else
    echo "Error: Test executable '$test_exe' not found or not executable" >&2
    exit 2
fi

# Clean up any previous test data
rm -rf pdc_tmp pdc_data

# Prepare log files for diagnostics
timestamp=$(date +%s)
server_log="server_log_$timestamp.txt"
client_log="client_log_$timestamp.txt"

# START the server (in the background) and redirect its output
echo "Starting server..." | tee "$server_log"
$run_cmd ./pdc_server >> "$server_log" 2>&1 &
server_pid=$!

# RUN the actual test
echo "Running client: $run_cmd $test_exe $test_args" | tee "$client_log"
$run_cmd $test_exe $test_args >> "$client_log" 2>&1
client_ret=$?

# Shut down the server (always attempt it)
echo "Shutting down server..." | tee -a "$server_log"
$run_cmd ./close_server >> "$server_log" 2>&1

# Wait for the server to fully exit
wait $server_pid
server_ret=$?

# Determine if the test passed
if [ $server_ret -ne 0 ]; then
    echo "❌ Server failed with exit code $server_ret" >&2
fi

if [ $client_ret -ne 0 ]; then
    echo "❌ Client failed with exit code $client_ret" >&2
fi

if [ $server_ret -ne 0 ] || [ $client_ret -ne 0 ]; then
    echo "📄 See logs:"
    echo "    Server log: $server_log"
    echo "    Client log: $client_log"
    exit 3
fi

echo "✅ Test succeeded"
exit 0

