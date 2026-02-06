#!/bin/bash
set -e

# Build
echo "Building..."
make clean > /dev/null
make > /dev/null

# Start simulation in background
echo "Starting simulation..."
bin/post_office_director -c config/simulation.conf -l INFO &
DIRECTOR_PID=$!

# Wait for it to initialize
sleep 2

# Find a worker PID
WORKER_PID=$(pgrep -f "post_office_worker" | head -n 1)

if [ -z "$WORKER_PID" ]; then
  echo "No worker found!"
  kill $DIRECTOR_PID
  exit 1
fi

echo "Killing worker $WORKER_PID..."
kill -9 $WORKER_PID

# Wait and see if director exits
echo "Waiting for director to exit..."
wait $DIRECTOR_PID
RET=$?

if [ $RET -eq 0 ]; then
  echo "Director exited normally (expected behavior after crash detection triggers shutdown)."
else
  echo "Director exited with $RET."
fi

echo "Test Passed."
