import React from 'react';
import { Timer } from 'lucide-react';

const ETAPanel = ({ eta }) => {
  const maxEta = 60;
  const progress = (eta / maxEta) * 100;
  
  return (
    <div className="glass-panel col-span-6 fade-enter">
      <div className="flex-between">
        <div className="label">ETA to Threshold</div>
        <Timer size={20} color="var(--accent-teal)" />
      </div>
      
      <div style={{ display: 'flex', alignItems: 'center', gap: '24px', marginTop: '1rem' }}>
        <div style={{ position: 'relative', width: '80px', height: '80px' }}>
          <svg viewBox="0 0 36 36" style={{ width: '100%', height: '100%', transform: 'rotate(-90deg)' }}>
            <path
              d="M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831"
              fill="none"
              stroke="var(--glass-highlight)"
              strokeWidth="3"
            />
            <path
              d="M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831"
              fill="none"
              stroke="var(--accent-cyan)"
              strokeWidth="3"
              strokeDasharray={`${progress}, 100`}
              style={{ transition: 'stroke-dasharray 1s ease' }}
            />
          </svg>
          <div style={{ 
            position: 'absolute', top: 0, left: 0, width: '100%', height: '100%', 
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            fontWeight: '700', fontSize: '1.2rem'
          }}>
            {eta}s
          </div>
        </div>
        
        <div>
          <h3 style={{ fontSize: '1.2rem', fontWeight: '600', marginBottom: '4px' }}>Updating Soon</h3>
          <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', lineHeight: '1.4' }}>
            The system is predicting the next threshold crossing based on the current thermal trajectory.
          </p>
        </div>
      </div>
    </div>
  );
};

export default ETAPanel;
