#!/usr/bin/env bash

echo "Killing all node processes..."

U="wolke"
pkill -u "$U" -f "./main"

echo "All nodes terminated."

