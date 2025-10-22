const express = require('express');
const dsa = require('../lib/cpp_dsa_client');
const router = express.Router();
router.post('/', (req,res)=>{
  const { priority, name } = req.body || {};
  if(typeof priority === 'undefined' || !name) return res.status(400).json({ error: 'priority & name required' });
  dsa.sendCommand('EMG_PUSH',[String(priority), String(name)]).then(r=>{ return res.json({ ok: true }); }).catch(e=>res.status(500).json({ error: 'dsa push failed' }));
});
router.get('/next', (req,res)=>{
  dsa.sendCommand('EMG_POP', []).then(r=>{ if(r[1]==='EMPTY') return res.json({ next: null }); return res.json({ priority: parseInt(r[1],10), name: r[2] }); }).catch(e=>res.status(500).json({ error: 'dsa pop failed' }));
});
module.exports = router;
