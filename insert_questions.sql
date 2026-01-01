-- Insert 50 questions into questions table
-- Distribution: 50% Easy, 30% Medium, 20% Hard
-- Topics: network_programming(1), oop(2), intro_to_ai(3), software(4), database(5)
-- Difficulties: easy(1), medium(2), hard(3)

-- First insert topics
INSERT INTO topics (id, name) VALUES
(1, 'network_programming'),
(2, 'oop'),
(3, 'intro_to_ai'),
(4, 'software'),
(5, 'database');

-- NETWORK_PROGRAMMING (10 questions)
-- Easy (5)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What does TCP stand for?', 'Transmission Control Protocol', 'Transfer Control Protocol', 'Transmission Connection Protocol', 'Transfer Connection Protocol', 'A', 1, 1),
('Which protocol is used for sending emails?', 'SMTP', 'HTTP', 'FTP', 'DNS', 'A', 1, 1),
('What is the default port for HTTP?', '80', '443', '8080', '3306', 'A', 1, 1),
('What does IP stand for?', 'Internet Protocol', 'Internal Protocol', 'Internet Process', 'Internal Process', 'A', 1, 1),
('Which layer of the OSI model does TCP belong to?', 'Transport Layer', 'Application Layer', 'Network Layer', 'Data Link Layer', 'A', 1, 1);

-- Medium (3)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What is the purpose of the three-way handshake in TCP?', 'Establish a reliable connection', 'Encrypt data transmission', 'Route packets efficiently', 'Balance network load', 'A', 1, 2),
('Which of the following is a connection-oriented protocol?', 'TCP', 'UDP', 'ICMP', 'ARP', 'A', 1, 2),
('What is the maximum size of an IPv4 address in bits?', '32 bits', '64 bits', '128 bits', '16 bits', 'A', 1, 2);

-- Hard (2)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('Explain the difference between TCP and UDP in terms of reliability', 'TCP is reliable, UDP is not', 'UDP is more reliable', 'Both are equally reliable', 'Neither is reliable', 'A', 1, 3),
('What is the purpose of the sliding window protocol in TCP?', 'Flow control and congestion management', 'Encryption of data', 'Route optimization', 'Load balancing', 'A', 1, 3);

-- OOP (10 questions)
-- Easy (5)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What does OOP stand for?', 'Object-Oriented Programming', 'Object-Operated Programming', 'Organized Operation Programming', 'Optional Object Programming', 'A', 2, 1),
('Which of these is a pillar of OOP?', 'Encapsulation', 'Decryption', 'Compilation', 'Interpretation', 'A', 2, 1),
('What is a class in OOP?', 'A blueprint for creating objects', 'A function that returns values', 'A collection of variables', 'A type of loop', 'A', 2, 1),
('What is inheritance in OOP?', 'Acquiring properties from a parent class', 'Creating new functions', 'Copying code multiple times', 'Organizing files', 'A', 2, 1),
('What does polymorphism mean?', 'Many forms', 'Single form', 'No form', 'Simple form', 'A', 2, 1);

-- Medium (3)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What is the difference between method overloading and method overriding?', 'Overloading is compile-time, overriding is runtime', 'They are the same', 'Overloading is runtime only', 'Overriding cannot happen in OOP', 'A', 2, 2),
('What is abstraction in OOP?', 'Hiding complex implementation details', 'Making everything public', 'Removing all methods', 'Creating abstract classes only', 'A', 2, 2),
('What is the purpose of access modifiers in OOP?', 'Control access to class members', 'Increase code speed', 'Make code shorter', 'Reduce memory usage', 'A', 2, 2);

-- Hard (2)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('Explain the concept of diamond problem in multiple inheritance', 'Ambiguity when inheriting from multiple classes with same method', 'A shape in design patterns', 'An optimization technique', 'A memory management issue', 'A', 2, 3),
('What is the difference between composition and inheritance?', 'Composition uses "has-a", inheritance uses "is-a" relationship', 'They are identical', 'Composition is faster', 'Inheritance is always better', 'A', 2, 3);

-- INTRO_TO_AI (10 questions)
-- Easy (5)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What does AI stand for?', 'Artificial Intelligence', 'Automated Integration', 'Applied Integration', 'Algorithmic Implementation', 'A', 3, 1),
('Which of these is an example of AI?', 'ChatGPT', 'Calculator', 'Text Editor', 'File Manager', 'A', 3, 1),
('What is machine learning?', 'A subset of AI that learns from data', 'A type of programming language', 'A hardware component', 'A database system', 'A', 3, 1),
('What is a neural network inspired by?', 'The human brain', 'Computer circuits', 'Mathematical equations', 'Database structure', 'A', 3, 1),
('Which technique is used in supervised learning?', 'Learning from labeled data', 'Learning without any data', 'Learning from unlabeled data', 'Learning from random patterns', 'A', 3, 1);

-- Medium (3)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What is the difference between supervised and unsupervised learning?', 'Supervised has labeled data, unsupervised does not', 'They are the same', 'Unsupervised is faster', 'Supervised cannot learn patterns', 'A', 3, 2),
('What is overfitting in machine learning?', 'Model learns training data too well, performs poorly on new data', 'Using too much training data', 'Model cannot learn anything', 'Using multiple models together', 'A', 3, 2),
('What is a decision tree in AI?', 'A tree-like structure for decision making based on features', 'A type of plant', 'A visualization of neural networks', 'A database query method', 'A', 3, 2);

-- Hard (2)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('Explain the concept of backpropagation in neural networks', 'Algorithm to compute gradients by propagating errors backward', 'Moving data forward through network', 'Removing unnecessary neurons', 'A method to store data', 'A', 3, 3),
('What is the purpose of activation functions in neural networks?', 'Introduce non-linearity to enable learning complex patterns', 'Reduce computation speed', 'Increase data storage', 'Create random outputs', 'A', 3, 3);

-- SOFTWARE (10 questions)
-- Easy (5)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What is the SDLC?', 'Software Development Life Cycle', 'System Design Logic Circuit', 'Software Design Logic Component', 'System Development Language Code', 'A', 4, 1),
('Which phase comes first in SDLC?', 'Planning', 'Implementation', 'Testing', 'Deployment', 'A', 4, 1),
('What is a bug in software?', 'An error or defect in the code', 'A feature that is too small', 'A type of user interface', 'A programming language', 'A', 4, 1),
('What is version control?', 'System to track changes in code', 'Controlling software versions released', 'A backup system', 'A testing framework', 'A', 4, 1),
('What does Git do?', 'Tracks code changes and enables collaboration', 'Compiles code', 'Runs tests automatically', 'Deploys applications', 'A', 4, 1);

-- Medium (3)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What is the difference between Waterfall and Agile methodologies?', 'Waterfall is linear, Agile is iterative', 'They are identical', 'Agile is slower', 'Waterfall is more flexible', 'A', 4, 2),
('What is code review in software development?', 'Process of examining code before merging to catch issues', 'A type of testing', 'Reviewing software licenses', 'Planning development tasks', 'A', 4, 2),
('What is refactoring?', 'Improving code without changing functionality', 'Removing all comments', 'Adding new features', 'Fixing critical bugs', 'A', 4, 2);

-- Hard (2)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('Explain the concept of technical debt in software development', 'Shortcuts taken that create future costs when fixing', 'Actual financial debt of the company', 'Unused code in the project', 'Deprecated libraries', 'A', 4, 3),
('What is CI/CD in software development?', 'Continuous Integration/Continuous Deployment for automated testing and deployment', 'Code Integration/Code Distribution', 'Continuous Improvement/Code Development', 'Client Interface/Code Delivery', 'A', 4, 3);

-- DATABASE (10 questions)
-- Easy (5)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What is a database?', 'Organized collection of structured data', 'A type of software application', 'A programming language', 'A file storage system', 'A', 5, 1),
('What does SQL stand for?', 'Structured Query Language', 'System Query Language', 'Sequential Query Logic', 'Simple Question Language', 'A', 5, 1),
('What is a primary key?', 'Unique identifier for each record', 'The most important data column', 'A password for database access', 'A type of index', 'A', 5, 1),
('What is a foreign key?', 'A key that links to a primary key in another table', 'A backup key for security', 'An encryption key', 'A temporary database key', 'A', 5, 1),
('What is normalization in databases?', 'Process of organizing data to reduce redundancy', 'Converting data to numeric format', 'Backing up database files', 'Compressing database size', 'A', 5, 1);

-- Medium (3)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('What is the difference between INNER JOIN and LEFT JOIN?', 'INNER returns matching rows, LEFT includes unmatched rows from left table', 'They are the same', 'LEFT is faster', 'INNER handles more data', 'A', 5, 2),
('What is an index in a database?', 'Data structure to speed up data retrieval', 'A list of all records', 'A backup of the database', 'A type of constraint', 'A', 5, 2),
('What is the purpose of transactions in databases?', 'Ensure data consistency by executing operations atomically', 'Store data safely', 'Backup data regularly', 'Compress database', 'A', 5, 2);

-- Hard (2)
INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id) VALUES
('Explain ACID properties in database transactions', 'Atomicity, Consistency, Isolation, Durability ensure reliable transactions', 'A chemical property of data', 'A type of encryption', 'A backup method', 'A', 5, 3),
('What is the difference between SQL and NoSQL databases?', 'SQL is relational with schemas, NoSQL is non-relational and flexible', 'They are identical', 'NoSQL is always faster', 'SQL cannot store complex data', 'A', 5, 3);
