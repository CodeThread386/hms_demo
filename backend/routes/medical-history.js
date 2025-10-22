const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const dsa = require('../lib/cpp_dsa_client');
const db = new sqlite3.Database(path.join(__dirname,'..','hospital.db'));
const router = express.Router();

function tokenFromHeader(h){ 
  if(!h) return null; 
  if(h.startsWith('Bearer ')) return h.slice(7); 
  return null; 
}

// Add medical record
router.post('/', (req,res)=>{
  const token = tokenFromHeader(req.headers.authorization);
  const { diagnosis, prescription, notes } = req.body || {};
  
  if(!token) return res.status(401).json({ error: 'missing token' });
  if(!diagnosis) return res.status(400).json({ error: 'diagnosis required' });
  
  dsa.sendCommand('SESSION_GET',[token]).then(resp=>{
    if(resp[0] !== 'OK') return res.status(401).json({ error: 'session error' });
    const uid = parseInt(resp[1]||'-1', 10);
    if(uid<0) return res.status(401).json({ error: 'invalid session' });
    
    const date = new Date().toISOString();
    db.run('INSERT INTO medical_history (user_id, diagnosis, prescription, notes, date) VALUES (?,?,?,?,?)', 
      [uid, diagnosis, prescription||'', notes||'', date], 
      function(err){
        if(err) return res.status(500).json({ error: err.message });
        const record_id = this.lastID;
        
        // Store in C++ DSA (BST indexed by record_id)
        const payload = `${uid}:${diagnosis}:${date}`;
        dsa.sendCommand('HISTORY_INSERT',[String(record_id), payload])
          .then(()=>{ 
            return res.json({ 
              record_id, 
              user_id: uid, 
              diagnosis, 
              prescription, 
              notes, 
              date 
            }); 
          })
          .catch(e=>{ 
            console.error('dsa.history_insert err', e); 
            return res.json({ 
              record_id, 
              diagnosis, 
              prescription, 
              notes, 
              date,
              note: 'DSA update failed' 
            }); 
          });
      }
    );
  }).catch(e=>res.status(500).json({ error: 'session lookup failed' }));
});

// Get medical history for user
router.get('/', (req,res)=>{
  const token = tokenFromHeader(req.headers.authorization);
  if(!token) return res.status(401).json({ error: 'missing token' });
  
  dsa.sendCommand('SESSION_GET',[token]).then(resp=>{
    if(resp[0] !== 'OK') return res.status(401).json({ error: 'session error' });
    const uid = parseInt(resp[1]||'-1', 10);
    if(uid<0) return res.status(401).json({ error: 'invalid session' });
    
    db.all('SELECT * FROM medical_history WHERE user_id = ? ORDER BY date DESC', [uid], (err,rows)=>{
      if(err) return res.status(500).json({ error: err.message });
      res.json({ history: rows || [] });
    });
  }).catch(e=>res.status(500).json({ error: 'session lookup failed' }));
});

module.exports = router;