CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL,
    role TEXT DEFAULT 'patient'
);

CREATE TABLE IF NOT EXISTS doctors (
    doctor_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    specialization TEXT,
    availability TEXT
);

CREATE TABLE IF NOT EXISTS appointments (
    appointment_id INTEGER PRIMARY KEY,
    user_id INTEGER,
    doctor_id INTEGER,
    time TEXT,
    status TEXT DEFAULT 'pending',
    FOREIGN KEY (user_id) REFERENCES users(user_id),
    FOREIGN KEY (doctor_id) REFERENCES doctors(doctor_id)
);

-- Insert some sample data
INSERT INTO users (username, password, role) VALUES 
('admin', 'admin123', 'admin'),
('doctor1', 'doc123', 'doctor'),
('patient1', 'pat123', 'patient');

INSERT INTO doctors (name, specialization, availability) VALUES 
('Dr. Smith', 'Cardiology', 'Mon-Fri'),
('Dr. Jones', 'Pediatrics', 'Mon-Wed'),
('Dr. Wilson', 'Orthopedics', 'Thu-Sat');
