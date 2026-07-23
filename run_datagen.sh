#!/bin/bash

# Create data folder and a new logs folder for text output
mkdir -p data logs

TARGET_POSITIONS=3000000000
THREADS=16

# Calculate positions needed per thread (187,500,000)
POS_PER_THREAD=$((TARGET_POSITIONS / THREADS))

echo "Starting 16-Thread Datagen for 3 Billion Positions (5,000 Nodes)..."
echo "Initializing engine instances..."

# 1. Launch the 16 threads and pipe their text output to individual log files
for i in {1..16}
do
   # Send the text output to a log file instead of /dev/null
   ./cwtch "datagen data $POS_PER_THREAD" > "logs/thread_${i}.log" 2>&1 &
   echo "Thread $i launched..."
done

echo "All 16 threads are running! Starting live progress tracker..."
echo "--------------------------------------------------------------------------------"

# 2. Ultra-Efficient Live Progress Tracker
while true; do
    # Check if the threads are still running
    if ! pgrep -x "cwtch" > /dev/null
    then
        echo -e "\nAll Cwtch threads have finished!"
        break
    fi

    POS_SUM=0
    GAMES_SUM=0
    SPEED_SUM=0

    # Quickly read the very last line of each log file
    for log in logs/thread_*.log; do
        if [ -f "$log" ]; then
            # Extract the last line containing "datagen:"
            LAST_LINE=$(tail -n 2 "$log" 2>/dev/null | grep "datagen:" | tail -n 1)
            
            if [ -n "$LAST_LINE" ]; then
                # Use awk to pull the exact numbers out of the engine's text string
                # Cwtch format: datagen: 5236/187500000 positions (0.0%) 46 games 257 pos/s
                read pos games speed <<< $(echo "$LAST_LINE" | awk '{split($2, a, "/"); print a[1], $5, $7}')
                
                # Add them to the master total
                POS_SUM=$((POS_SUM + pos))
                GAMES_SUM=$((GAMES_SUM + games))
                SPEED_SUM=$((SPEED_SUM + speed))
            fi
        fi
    done

    # Calculate percentage safely with awk
    PERCENTAGE=$(awk -v pos="$POS_SUM" -v target="$TARGET_POSITIONS" 'BEGIN { if(target>0) printf "%.2f", (pos / target) * 100; else print "0.00" }')

    # Print the master stats including Games and combined Speed!
    printf "\rProgress: %'d / %'d pos (%s%%) | %'d Games | Speed: %'d pos/s   " "$POS_SUM" "$TARGET_POSITIONS" "$PERCENTAGE" "$GAMES_SUM" "$SPEED_SUM"
    
    # Update every 10 seconds to keep disk I/O completely free for the engine
    sleep 10
done
