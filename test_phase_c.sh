#!/bin/bash

# Phase C Test Script - Room Creation and Question Loading

echo "=== PHASE C TEST: Room Creation & Question Loading ==="
echo

# Kill old server
pkill -9 -f "./server" 2>/dev/null
sleep 1

# Fresh database
rm -f test_system.db*

# Start server
echo "Starting server..."
./server > /tmp/server.log 2>&1 &
SERVER_PID=$!
sleep 2

# Check server log for initialization
echo "Server initialization output:"
head -15 /tmp/server.log
echo

# Test GET_TOPICS command
echo "Testing GET_TOPICS command..."
echo "GET_TOPICS" | nc localhost 9000 2>/dev/null | head -5
echo

# Test CREATE_ROOM command (need topics and difficulties)
echo "Testing CREATE_ROOM command..."
echo -e "CREATE admin admin 2 60 TOPICS cloud:1 database:1 DIFFICULTIES easy:2" | nc localhost 9000 2>/dev/null | head -5
echo

# Query database directly to verify data
echo "Verifying database contents..."
echo "
SELECT 'Topics count:', COUNT(*) FROM topics;
SELECT 'Questions count:', COUNT(*) FROM questions;
SELECT 'Rooms count:', COUNT(*) FROM rooms;
SELECT 'Room questions count:', COUNT(*) FROM room_questions;
SELECT 'Users count:', COUNT(*) FROM users;
" | sqlite3 test_system.db
echo

# Cleanup
kill $SERVER_PID 2>/dev/null
echo "Test complete!"
