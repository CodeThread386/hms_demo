const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const db = new sqlite3.Database(path.join(__dirname,'..','hospital.db'));
const router = express.Router();
router.get('/', (req,res)=>{
  db.all('SELECT doctor_id, name, specialization FROM doctors ORDER BY doctor_id', [], (err, rows)=>{
    if(err) return res.status(500).json({ error: err.message });
    res.json({ doctors: rows });
  });
});
module.exports = router;
