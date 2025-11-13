#!/usr/bin/env bash

BASE_PORT=52000
NUM_NODES=4

# flags
while [[ $# -gt 0 ]]; do
    case "$1" in
        --bp)
            BASE_PORT="$2"
            shift 2
            ;;
        --n)
            NUM_NODES="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--bp N] [--n N]"
            exit 1
            ;;
    esac
done


for ((i=1; i<=NUM_NODES; i++)); do
    id=$i
    ip="localhost"
    port=$((BASE_PORT + i - 1))

    # define peer as the next node (wrap around)
    if [ $i -lt $NUM_NODES ]; then
        peer_id=$((i + 1))
    else
        peer_id=1
    fi

    peer_ip="localhost"
    peer_port=$((BASE_PORT + peer_id - 1))

    echo "Starting node $id on port $port (peer: $peer_id@$peer_ip:$peer_port)"

    ./main \
        -id "$id" \
        -ip "$ip:$port" \
        -peer-id "$peer_id" \
        -peer-ip "$peer_ip:$peer_port" \
        -network-size "$NUM_NODES" \
        > "logs/node_${id}.log" 2>&1 &
done

echo "All nodes started. Logs: node_*.log"
