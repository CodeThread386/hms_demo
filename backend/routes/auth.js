const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const bcrypt = require('bcrypt');
const { v4: uuidv4 } = require('uuid');
const path = require('path');
const dsa = require('../lib/cpp_dsa_client');
const db = new sqlite3.Database(path.join(__dirname,'..','hospital.db'));
const router = express.Router();
router.post('/register', (req,res)=>{
  const { username, password, email } = req.body || {};
  if(!username || !password) return res.status(400).json({ error:'username & password required' });
  bcrypt.hash(password,10).then(hash=>{
    db.run('INSERT INTO users (username,password_hash,user_type,email) VALUES (?,?,?,?)',[username,hash,'patient',email], function(err){
      if(err) return res.status(500).json({ error: err.message });
      const user_id = this.lastID;
      dsa.sendCommand('REGISTER',[username,String(user_id)]).then(r=>{ return res.json({ user_id, username, email }); }).catch(e=>{ console.error('dsa.register err', e); return res.json({ user_id, username, email, note:'registered but DSA update failed' }); });
    });
  }).catch(err=>res.status(500).json({ error: err.message }));
});
router.post('/login', (req,res)=>{
  const { username, password } = req.body || {};
  if(!username||!password) return res.status(400).json({ error:'username & password required' });
  db.get('SELECT user_id, username, password_hash, user_type, email FROM users WHERE username = ?', [username], async (err,row)=>{
    if(err) return res.status(500).json({ error: err.message });
    if(!row) return res.status(401).json({ error: 'Invalid credentials' });
    const ok = await bcrypt.compare(password, row.password_hash);
    if(!ok) return res.status(401).json({ error: 'Invalid credentials' });
    const token = uuidv4();
    dsa.sendCommand('SESSION_PUT',[token,String(row.user_id)]).then(()=>{ return res.json({ token, user: { user_id: row.user_id, username: row.username, user_type: row.user_type, email: row.email } }); }).catch(e=>{ console.error('dsa.session_put err', e); return res.json({ token, user: { user_id: row.user_id, username: row.username, user_type: row.user_type, email: row.email }, note:'session map update failed' }); });
  });
});
module.exports = router;
