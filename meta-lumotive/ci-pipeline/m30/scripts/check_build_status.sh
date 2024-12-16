#!/bin/bash

# Check if a log file path is provided as an argument
if [ $# -eq 0 ]; then
  echo "Usage: $0 path/to/logfile.log"
  exit 1
fi

# Use the first argument as the log file path
LOG_FILE="$1"

# Print the log file
echo "Monitoring build log file: $LOG_FILE"
cat "$LOG_FILE"

# Define the success and failure messages to look for
SUCCESS_MESSAGE="didn't need to be rerun and all succeeded"
FAILURE_MESSAGE="ERROR:"

# Create a temporary file to act as a flag for termination
FLAG_FILE=$(mktemp)

# Start tailing the log file and send the output to both stdout and a subshell
tail -f "$LOG_FILE" | tee >( \
  while IFS= read -r LINE; do
    
    # Check for success message
    if [[ "$LINE" =~ $SUCCESS_MESSAGE ]]; then
      echo "Build completed successfully."
      echo "success" > "$FLAG_FILE" # Indicate success in the flag file
      break
    fi
    
    # Check for failure message
    if [[ "$LINE" =~ $FAILURE_MESSAGE ]]; then
      echo "Build failed."
      echo "failure" > "$FLAG_FILE" # Indicate failure in the flag file
      break
    fi
  done
) &

# Capture the PID of the tail command
TAIL_PID=$!

# Monitor the flag file for changes
inotifywait -e close_write "$FLAG_FILE"

# Read the flag file to determine the exit status
BUILD_STATUS=$(<"$FLAG_FILE")

# Check if tail process exists before terminating it to avoid errors
ps -p "$TAIL_PID" > /dev/null
TAIL_EXISTS=$?
if [ $TAIL_EXISTS -eq 0 ]; then
  echo "Terminating tail process and stopping stream of log file."
  # Terminate the tail process
  kill "$TAIL_PID" 
else
  # This can happen because the yocto build in the background errors out 
  # to stdout and Bitbucket pipelines kills the step based on stdout or pipelines terminating early because of a successful build 
  echo "Tail process has already terminated. Possibly due to a Build failure or pipeline terminating early because of a successful build."
fi

# Clean up the flag file
rm "$FLAG_FILE"

# Exit based on the build status
if [ "$BUILD_STATUS" = "success" ]; then
  echo "Build monitoring terminated successfully."
  exit 0
else
  echo "Build monitoring detected a failure."
  exit 1
fi