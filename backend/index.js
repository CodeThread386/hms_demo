const express = require('express');
const bodyParser = require('body-parser');
const cors = require('cors');
const authRoutes = require('./routes/auth');
const doctorsRoutes = require('./routes/doctors');
const apptRoutes = require('./routes/appointments');
const emgRoutes = require('./routes/emergency');
const historyRoutes = require('./routes/medical-history');
const statsRoutes = require('./routes/stats');

const app = express();

app.use(cors());
app.use(bodyParser.json());

// Routes
app.use('/api/auth', authRoutes);
app.use('/api/doctors', doctorsRoutes);
app.use('/api/appointments', apptRoutes);
app.use('/api/emergency', emgRoutes);
app.use('/api/medical-history', historyRoutes);
app.use('/api/stats', statsRoutes);

const PORT = process.env.PORT || 4000;
app.listen(PORT, () => {
  console.log(`✅ Node backend running on http://localhost:${PORT}`);
  console.log(`📡 API endpoints available at http://localhost:${PORT}/api`);
});