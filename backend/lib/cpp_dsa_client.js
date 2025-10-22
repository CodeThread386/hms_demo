const net = require('net');
function escapeField(s){ return s.replace(/\\/g,'\\\\').replace(/\|/g,'\\|'); }
function sendCommand(cmd, args=[], host='127.0.0.1', port=4001, timeout=4000){
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    let done=false; const dataParts=[];
    client.setTimeout(timeout, ()=>{ if(!done){ done=true; client.destroy(); reject(new Error('DSA service timeout')); } });
    client.connect(port, host, ()=>{ const line = [cmd, ...args.map(escapeField)].join('|') + '\n'; client.write(line); });
    client.on('data', (data)=>{ dataParts.push(data.toString('utf8')); });
    client.on('end', ()=>{ if(done) return; done=true; const resp = dataParts.join(''); const parts = []; let cur=''; let esc=false; for(let i=0;i<resp.length;i++){ const c = resp[i]; if(c=='\n') break; if(esc){ cur+=c; esc=false; continue; } if(c=='\\'){ esc=true; continue; } if(c=='|'){ parts.push(cur); cur=''; continue; } cur+=c; } parts.push(cur); resolve(parts); client.destroy(); });
    client.on('error', (err)=>{ if(done) return; done=true; reject(err); });
  });
}
module.exports = { sendCommand };
