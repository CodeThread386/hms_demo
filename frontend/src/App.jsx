import React from 'react'
import { Routes, Route, Navigate } from 'react-router-dom'
import ModernHospitalSystem from './components/ModernHospitalSystem'

const App = () => {
  return (
    <Routes>
      <Route path="/" element={<ModernHospitalSystem />} />
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>
  )
}

export default App
