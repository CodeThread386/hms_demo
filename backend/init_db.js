const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const bcrypt = require('bcrypt');
const dbPath = path.join(__dirname, 'hospital.db');
const db = new sqlite3.Database(dbPath);
function run(sql, params=[]) { return new Promise((res, rej) => db.run(sql, params, function(err){ if(err) return rej(err); res(this); })); }
(async () => {
  try {
    await run(`CREATE TABLE IF NOT EXISTS users (
      user_id INTEGER PRIMARY KEY AUTOINCREMENT,
      username TEXT UNIQUE,
      password_hash TEXT,
      user_type TEXT,
      email TEXT
    );`);
    await run(`CREATE TABLE IF NOT EXISTS doctors (
      doctor_id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT,
      specialization TEXT
    );`);
    await run(`CREATE TABLE IF NOT EXISTS appointments (
      appointment_id INTEGER PRIMARY KEY AUTOINCREMENT,
      user_id INTEGER,
      doctor_id INTEGER,
      time TEXT,
      status TEXT DEFAULT 'booked',
      FOREIGN KEY(user_id) REFERENCES users(user_id),
      FOREIGN KEY(doctor_id) REFERENCES doctors(doctor_id)
    );`);
    const adminHash = await bcrypt.hash('admin123', 10);
    await run(`INSERT OR IGNORE INTO users (user_id, username, password_hash, user_type, email) VALUES (1, 'admin', ?, 'admin', 'admin@hospital.com')`, [adminHash]);
    await run(`INSERT OR IGNORE INTO doctors (doctor_id, name, specialization) VALUES (1, 'Dr. Alice Smith', 'General')`);
    console.log('DB initialized at', dbPath);
    process.exit(0);
  } catch (err) {
    console.error('init error', err);
    process.exit(1);
  }
})();
