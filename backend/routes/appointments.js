const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const dsa = require('../lib/cpp_dsa_client');
const db = new sqlite3.Database(path.join(__dirname,'..','hospital.db'));
const router = express.Router();
function tokenFromHeader(h){ if(!h) return null; if(h.startsWith('Bearer ')) return h.slice(7); return null; }
router.post('/book', (req,res)=>{
  const token = tokenFromHeader(req.headers.authorization);
  const { doctor_id, time } = req.body || {};
  if(!token) return res.status(401).json({ error: 'missing token' });
  dsa.sendCommand('SESSION_GET',[token]).then(resp=>{
    if(resp[0] !== 'OK') return res.status(401).json({ error: 'session error' });
    const uid = parseInt(resp[1]||'-1', 10);
    if(uid<0) return res.status(401).json({ error: 'invalid session' });
    db.run('INSERT INTO appointments (user_id, doctor_id, time) VALUES (?,?,?)', [uid, doctor_id, time], function(err){
      if(err) return res.status(500).json({ error: err.message });
      const aid = this.lastID;
      dsa.sendCommand('APPT_INSERT',[String(aid), String(uid), String(doctor_id), String(time)]).then(()=>{ return res.json({ appointment_id: aid, doctor_id, time, status: 'booked' }); }).catch(e=>{ console.error('dsa.appt_insert err', e); return res.json({ appointment_id: aid, doctor_id, time, status: 'booked', note: 'DSA update failed' }); });
    });
  }).catch(e=>res.status(500).json({ error: 'session lookup failed' }));
});
router.get('/', (req,res)=>{
  const token = tokenFromHeader(req.headers.authorization);
  if(!token) return res.status(401).json({ error: 'missing token' });
  dsa.sendCommand('SESSION_GET',[token]).then(resp=>{
    if(resp[0] !== 'OK') return res.status(401).json({ error: 'session error' });
    const uid = parseInt(resp[1]||'-1', 10);
    if(uid<0) return res.status(401).json({ error: 'invalid session' });
    dsa.sendCommand('APPT_LIST_BY_USER',[String(uid)]).then(listResp=>{
      const csv = listResp[1] || '';
      if(csv==='') return res.json({ appointments: [] });
      const parts = csv.split(',');
      const out = []; let pending = parts.length;
      parts.forEach(p=>{
        const aid = parseInt(p.split(':')[0],10);
        db.get('SELECT appointment_id, user_id, doctor_id, time, status FROM appointments WHERE appointment_id = ?', [aid], (err,row)=>{
          if(row) out.push(row);
          pending--; if(pending===0) res.json({ appointments: out });
        });
      });
      if(parts.length===0) res.json({ appointments: [] });
    }).catch(e=>res.status(500).json({ error: 'DSA appt list failed' }));
  }).catch(e=>res.status(500).json({ error: 'session lookup failed' }));
});
module.exports = router;
