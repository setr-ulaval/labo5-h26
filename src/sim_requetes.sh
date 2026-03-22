#!/bin/bash
# sim_requetes.sh - Simule createurRequetes en local
PIPE=${1:-/tmp/test_pipe}
while true; do
    MSG=$(cat /dev/urandom | tr -dc 'A-Za-z0-9.' | head -c $((RANDOM % 11 + 5)))
    echo -ne "${MSG}\x04" > "$PIPE"
    sleep 0.$((RANDOM % 500 + 100))
done
