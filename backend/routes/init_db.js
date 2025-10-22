const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const bcrypt = require('bcrypt');

const dbPath = path.join(__dirname, 'hospital.db');
const db = new sqlite3.Database(dbPath);

function run(sql, params=[]) { 
  return new Promise((res, rej) => 
    db.run(sql, params, function(err){ 
      if(err) return rej(err); 
      res(this); 
    })
  ); 
}

(async () => {
  try {
    // Users table
    await run(`CREATE TABLE IF NOT EXISTS users (
      user_id INTEGER PRIMARY KEY AUTOINCREMENT,
      username TEXT UNIQUE NOT NULL,
      password_hash TEXT NOT NULL,
      user_type TEXT DEFAULT 'patient',
      email TEXT,
      phone TEXT,
      created_at TEXT DEFAULT CURRENT_TIMESTAMP
    );`);

    // Doctors table
    await run(`CREATE TABLE IF NOT EXISTS doctors (
      doctor_id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT NOT NULL,
      specialization TEXT NOT NULL,
      experience_years INTEGER DEFAULT 0,
      available BOOLEAN DEFAULT 1
    );`);

    // Appointments table
    await run(`CREATE TABLE IF NOT EXISTS appointments (
      appointment_id INTEGER PRIMARY KEY AUTOINCREMENT,
      user_id INTEGER NOT NULL,
      doctor_id INTEGER NOT NULL,
      time TEXT NOT NULL,
      reason TEXT,
      status TEXT DEFAULT 'booked',
      created_at TEXT DEFAULT CURRENT_TIMESTAMP,
      FOREIGN KEY(user_id) REFERENCES users(user_id),
      FOREIGN KEY(doctor_id) REFERENCES doctors(doctor_id)
    );`);

    // Medical history table
    await run(`CREATE TABLE IF NOT EXISTS medical_history (
      record_id INTEGER PRIMARY KEY AUTOINCREMENT,
      user_id INTEGER NOT NULL,
      diagnosis TEXT NOT NULL,
      prescription TEXT,
      notes TEXT,
      date TEXT NOT NULL,
      FOREIGN KEY(user_id) REFERENCES users(user_id)
    );`);

    // Create admin user
    const adminHash = await bcrypt.hash('admin123', 10);
    await run(
      `INSERT OR IGNORE INTO users (user_id, username, password_hash, user_type, email) 
       VALUES (1, 'admin', ?, 'admin', 'admin@hospital.com')`, 
      [adminHash]
    );

    // Insert sample doctors
    const sampleDoctors = [
      ['Dr. Alice Smith', 'Cardiology', 15],
      ['Dr. Bob Johnson', 'Neurology', 12],
      ['Dr. Carol Williams', 'Pediatrics', 10],
      ['Dr. David Brown', 'Orthopedics', 8],
      ['Dr. Emma Davis', 'Dermatology', 6],
      ['Dr. Frank Miller', 'General Medicine', 20],
      ['Dr. Grace Wilson', 'Gynecology', 14],
      ['Dr. Henry Taylor', 'Ophthalmology', 11]
    ];

    for(const [name, spec, exp] of sampleDoctors) {
      await run(
        `INSERT OR IGNORE INTO doctors (name, specialization, experience_years) 
         VALUES (?, ?, ?)`,
        [name, spec, exp]
      );
    }

    console.log('✅ Database initialized successfully at', dbPath);
    console.log('📊 Sample doctors added');
    process.exit(0);
  } catch (err) {
    console.error('❌ Database initialization error:', err);
    process.exit(1);
  }
})();