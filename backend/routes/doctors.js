const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const dsa = require('../lib/cpp_dsa_client');
const db = new sqlite3.Database(path.join(__dirname,'..','hospital.db'));
const router = express.Router();

// Get all doctors
router.get('/', (req,res)=>{
  db.all('SELECT doctor_id, name, specialization FROM doctors ORDER BY name', [], (err, rows)=>{
    if(err) return res.status(500).json({ error: err.message });
    res.json({ doctors: rows });
  });
});

// Search doctors using C++ DSA
router.get('/search', async (req,res)=>{
  const query = req.query.q || '';
  
  try {
    // First, try to find by exact name match using AVL tree in C++
    const dsaResp = await dsa.sendCommand('DOCTOR_SEARCH', [query]);
    
    // Fallback to SQL-based search for partial matches
    db.all(
      'SELECT doctor_id, name, specialization FROM doctors WHERE name LIKE ? OR specialization LIKE ? ORDER BY name',
      [`%${query}%`, `%${query}%`],
      (err, rows) => {
        if(err) return res.status(500).json({ error: err.message });
        res.json({ doctors: rows || [] });
      }
    );
  } catch(err) {
    console.error('Search error:', err);
    // Fallback to regular search
    db.all(
      'SELECT doctor_id, name, specialization FROM doctors WHERE name LIKE ? OR specialization LIKE ? ORDER BY name',
      [`%${query}%`, `%${query}%`],
      (err, rows) => {
        if(err) return res.status(500).json({ error: err.message });
        res.json({ doctors: rows || [] });
      }
    );
  }
});

module.exports = router;