#!/bin/bash
# Phase C1: Test room creation and question persistence

set -e

echo "====== Phase C1: Testing Room Creation & Database Persistence ======"
echo ""

# Clean slate
pkill -9 -f "./server" 2>/dev/null || true
sleep 1
rm -f test_system.db test_system.db-*

# Start server
echo "[1] Starting server..."
./server > /tmp/server.log 2>&1 &
SERVER_PID=$!
sleep 3

echo "[2] Server started (PID: $SERVER_PID)"
echo ""

# Test via sqlite3
echo "[3] Checking database state..."
echo ""

echo "Topics in database:"
sqlite3 test_system.db "SELECT id, name FROM topics ORDER BY id;"
echo ""

echo "Difficulties in database:"
sqlite3 test_system.db "SELECT id, name, level FROM difficulties ORDER BY id;"
echo ""

echo "Questions count per topic:"
sqlite3 test_system.db "SELECT t.name, d.name, COUNT(*) as count FROM questions q 
                        JOIN topics t ON q.topic_id = t.id 
                        JOIN difficulties d ON q.difficulty_id = d.id 
                        GROUP BY t.id, d.id 
                        ORDER BY t.id, d.id;"
echo ""

echo "Sample questions (first 3):"
sqlite3 test_system.db "SELECT id, substr(text, 1, 50) as question, topic_id, difficulty_id FROM questions LIMIT 3;"
echo ""

echo "Users in database:"
sqlite3 test_system.db "SELECT id, username, role FROM users;"
echo ""

echo "Rooms in database (should be empty):"
sqlite3 test_system.db "SELECT id, name, owner_id FROM rooms;"
echo ""

# Now test CREATE_ROOM protocol
echo "[4] Testing room creation via network protocol..."

# Create a simple client script to test CREATE_ROOM
cat > /tmp/test_client.sh << 'EOF'
#!/bin/bash

{
    # Read initial "Press Enter" message and skip it
    read -t 2 dummy || true
    
    # Should show menu - send "1" for login
    echo "1"
    sleep 0.5
    
    # Username prompt
    echo "admin"
    sleep 0.5
    
    # Password prompt  
    echo "admin123"
    sleep 1
    
    # Should be in menu now - send "3" for create room
    echo "3"
    sleep 0.5
    
    # Room name
    echo "TestRoom1"
    sleep 0.5
    
    # Number of questions
    echo "5"
    sleep 0.5
    
    # Duration
    echo "60"
    sleep 0.5
    
    # Topic selection (assuming first topic is cloud)
    echo "1"
    sleep 0.5
    
    # Wait for difficulty selection
    sleep 0.5
    
    # Difficulty selection (assuming easy)
    echo "1"
    sleep 0.5
    
    # Wait for server response
    sleep 1
    
    # Exit
    echo "0"
    
} | timeout 10 ./client 2>&1
EOF

chmod +x /tmp/test_client.sh
cd /home/khanh/Desktop/NetProgramming
/tmp/test_client.sh | tail -30

echo ""
echo "[5] Checking if room was created in database..."
echo ""

echo "Rooms in database (after CREATE_ROOM):"
sqlite3 test_system.db "SELECT id, name, owner_id, duration_minutes FROM rooms;"
echo ""

echo "Questions in room (room_questions table):"
sqlite3 test_system.db "SELECT room_id, question_id FROM room_questions;"
echo ""

# Cleanup
echo "[6] Cleanup..."
kill $SERVER_PID 2>/dev/null || true
sleep 1

echo ""
echo "====== Phase C1 Test Complete ======"
