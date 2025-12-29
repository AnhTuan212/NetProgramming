-- ============================================================
-- RESET DATABASE - XÓA SẠch TẤT CẢ DỮ LIỆU & THÊM SAMPLE DATA
-- ============================================================

-- Disable foreign key checks temporarily
PRAGMA foreign_keys = OFF;

-- ===== TRUNCATE ALL DATA (in correct order to avoid foreign key issues) =====
DELETE FROM answers;
DELETE FROM participants;
DELETE FROM room_questions;
DELETE FROM rooms;
DELETE FROM questions;
DELETE FROM users;
DELETE FROM difficulties;
DELETE FROM topics;

-- Reset auto-increment IDs
DELETE FROM sqlite_sequence;

-- ===== RE-INSERT TOPICS =====
INSERT OR IGNORE INTO topics (name, description) VALUES ('database', 'Database and SQL');
INSERT OR IGNORE INTO topics (name, description) VALUES ('networking', 'Network and Communication');
INSERT OR IGNORE INTO topics (name, description) VALUES ('security', 'Cybersecurity and Encryption');
INSERT OR IGNORE INTO topics (name, description) VALUES ('programming', 'Programming Languages and Concepts');
INSERT OR IGNORE INTO topics (name, description) VALUES ('cloud', 'Cloud Computing');

-- ===== RE-INSERT DIFFICULTIES =====
INSERT OR IGNORE INTO difficulties (name, level) VALUES ('easy', 1);
INSERT OR IGNORE INTO difficulties (name, level) VALUES ('medium', 2);
INSERT OR IGNORE INTO difficulties (name, level) VALUES ('hard', 3);

-- ===== INSERT 30 SAMPLE QUESTIONS (5 topics x 6 questions each) =====
-- Topic IDs: 1=database, 2=networking, 3=security, 4=programming, 5=cloud
-- Difficulty IDs: 1=easy, 2=medium, 3=hard

-- TOPIC 1: DATABASE (topic_id=1, 6 questions: 3 easy, 2 medium, 1 hard)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What does SQL stand for?', 'Structured Query Language', 'Standard Query Language', 'Structured Quick Language', 'Standard Quick Language', 'A', 1, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Which keyword is used to sort data in SQL?', 'FILTER', 'SORT', 'ORDER BY', 'ARRANGE', 'C', 1, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is a PRIMARY KEY?', 'A key that opens the database', 'A unique identifier for each record', 'A temporary key', 'A key used for backup', 'B', 1, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Which SQL statement is used to insert data?', 'ADD', 'INSERT INTO', 'APPEND', 'PUT', 'B', 1, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is database normalization?', 'Backing up the database', 'Organizing data to reduce redundancy', 'Encrypting the database', 'Converting data to text', 'B', 1, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Explain the difference between INNER JOIN and LEFT JOIN in SQL', 'INNER JOIN returns matching records only, LEFT JOIN includes all records from left table', 'They are the same', 'LEFT JOIN is faster', 'INNER JOIN is used for updates', 'A', 1, 3, -1);

-- TOPIC 2: NETWORKING (topic_id=2, 6 questions: 3 easy, 2 medium, 1 hard)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What does TCP stand for?', 'Transfer Control Protocol', 'Transmission Control Protocol', 'Transfer Communication Protocol', 'Transparent Control Protocol', 'B', 2, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Which port is typically used for HTTP?', '25', '80', '443', '8080', 'B', 2, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is an IP address?', 'A website address', 'A numerical label for devices on a network', 'A type of email', 'A security protocol', 'B', 2, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is the purpose of a firewall?', 'To speed up the internet', 'To block unauthorized access and protect networks', 'To store data', 'To connect computers', 'B', 2, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is a subnet mask?', 'A type of password', 'An IP address filter that divides networks', 'A security certificate', 'A backup method', 'B', 2, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Explain the OSI model and its 7 layers', 'A model describing network communication in 7 layers: Physical, Data Link, Network, Transport, Session, Presentation, Application', 'A type of Internet provider', 'A security software', 'A routing algorithm', 'A', 2, 3, -1);

-- TOPIC 3: SECURITY (topic_id=3, 6 questions: 3 easy, 2 medium, 1 hard)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is encryption?', 'Storing data on a server', 'Converting data into a secret code', 'Backing up files', 'Deleting files', 'B', 3, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What does SSL/TLS stand for?', 'Secure Socket Layer / Transport Layer Security', 'Simple Secure Language', 'System Security Link', 'Secure Storage Locking', 'A', 3, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Which is a strong password?', 'password123', 'abc', 'P@ssw0rd!Secure2024', '12345', 'C', 3, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is two-factor authentication (2FA)?', 'Using the same password twice', 'A security method requiring two verification methods', 'Changing password twice a year', 'Two different devices', 'B', 3, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What are common cybersecurity threats?', 'Slow internet', 'Malware, phishing, ransomware, DDoS attacks', 'Old software', 'Too many users', 'B', 3, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Explain the concept of a hash function in security', 'A function that converts input to fixed-length code, used for password storage', 'A type of encryption', 'A backup method', 'A compression algorithm', 'A', 3, 3, -1);

-- TOPIC 4: PROGRAMMING (topic_id=4, 6 questions: 3 easy, 2 medium, 1 hard)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is a variable?', 'A named storage location in memory', 'A fixed value', 'A function', 'A loop', 'A', 4, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What does OOP stand for?', 'Organized Object Programming', 'Object-Oriented Programming', 'Open Oriented Process', 'Optimal Object Pattern', 'B', 4, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Which of these is a loop statement?', 'if', 'else', 'for', 'switch', 'C', 4, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is the difference between stack and queue?', 'Stack is LIFO, Queue is FIFO', 'They are the same', 'Stack is faster', 'Queue uses less memory', 'A', 4, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is a pointer in C?', 'A device connection', 'A variable that stores memory address of another variable', 'A type of loop', 'A function parameter', 'B', 4, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Explain polymorphism in OOP', 'The ability of objects to take multiple forms and be processed differently', 'Multiple programs', 'Many lines of code', 'Multiple inheritance', 'A', 4, 3, -1);

-- TOPIC 5: CLOUD (topic_id=5, 6 questions: 3 easy, 2 medium, 1 hard)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is cloud computing?', 'Using local computers', 'On-demand access to computing resources over the internet', 'A weather application', 'A storage device', 'B', 5, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What does IaaS stand for?', 'Internet as a Service', 'Infrastructure as a Service', 'Integration as a Service', 'Information as a Service', 'B', 5, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Which is a cloud service provider?', 'Microsoft Word', 'Amazon Web Services (AWS)', 'Google Chrome', 'Adobe Reader', 'B', 5, 1, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is the advantage of cloud computing?', 'Requires physical servers', 'Scalability and cost-effectiveness', 'Slower processing', 'Higher security risks', 'B', 5, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('What is a virtual machine in cloud computing?', 'A physical server', 'A software-based computer system that emulates physical computing hardware', 'A type of database', 'A network router', 'B', 5, 2, -1);

INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by)
VALUES ('Explain the difference between SaaS, PaaS, and IaaS', 'SaaS is software, PaaS is platform, IaaS is infrastructure delivered as cloud services', 'They are all the same', 'SaaS is the fastest', 'PaaS is the cheapest', 'A', 5, 3, -1);

-- ===== RE-ENABLE FOREIGN KEY CHECKS =====
PRAGMA foreign_keys = ON;

-- ===== VERIFICATION =====
SELECT 'Database Reset Complete!' as Status;
SELECT COUNT(*) as QuestionCount FROM questions;
SELECT DISTINCT topic, COUNT(*) as Count FROM questions GROUP BY topic;
SELECT difficulty, COUNT(*) as Count FROM questions GROUP BY difficulty;
