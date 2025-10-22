const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const dsa = require('../lib/cpp_dsa_client');
const db = new sqlite3.Database(path.join(__dirname,'..','hospital.db'));
const router = express.Router();

router.get('/', async (req,res)=>{
  try {
    // Get doctor count
    const doctorCount = await new Promise((resolve, reject) => {
      db.get('SELECT COUNT(*) as count FROM doctors', [], (err, row) => {
        if(err) reject(err);
        else resolve(row.count);
      });
    });

    // Get appointment count
    const apptCount = await new Promise((resolve, reject) => {
      db.get('SELECT COUNT(*) as count FROM appointments', [], (err, row) => {
        if(err) reject(err);
        else resolve(row.count);
      });
    });

    // Get emergency queue size from C++ DSA
    const emgResp = await dsa.sendCommand('EMG_SIZE', []);
    const emergencyCases = emgResp[0] === 'OK' ? parseInt(emgResp[1] || '0', 10) : 0;

    res.json({
      totalDoctors: doctorCount,
      totalAppointments: apptCount,
      emergencyCases: emergencyCases
    });
  } catch(err) {
    console.error('Stats error:', err);
    res.status(500).json({ error: 'Failed to fetch stats' });
  }
});

module.exports = router;