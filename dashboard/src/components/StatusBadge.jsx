import React from 'react';
import { ShieldCheck, ShieldAlert, TrendingUp, TrendingDown, Minus } from 'lucide-react';

const StatusBadge = ({ quality, trend }) => {
  const getQualityColor = () => {
    if (quality === 'High') return 'var(--accent-green)';
    if (quality === 'Med') return 'var(--accent-warning)';
    return 'var(--accent-danger)';
  };

  const getTrendIcon = () => {
    if (trend === 'Rising') return <TrendingUp size={20} color="var(--accent-warning)" />;
    if (trend === 'Falling') return <TrendingDown size={20} color="var(--accent-cyan)" />;
    return <Minus size={20} color="var(--text-muted)" />;
  };

  return (
    <div className="glass-panel col-span-6 fade-enter" style={{ display: 'flex', flexDirection: 'column', gap: '1.5rem' }}>
      <div className="label">Status & Trend</div>
      
      <div className="flex-between">
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ 
            width: '40px', height: '40px', borderRadius: '10px', 
            background: 'var(--glass-highlight)', display: 'flex', alignItems: 'center', justifyContent: 'center' 
          }}>
            {quality === 'Low' ? <ShieldAlert size={20} color={getQualityColor()} /> : <ShieldCheck size={20} color={getQualityColor()} />}
          </div>
          <div>
            <div style={{ fontSize: '0.8rem', color: 'var(--text-muted)' }}>Signal Quality</div>
            <div style={{ fontWeight: '600', color: getQualityColor() }}>{quality}</div>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
           <div style={{ 
            width: '40px', height: '40px', borderRadius: '10px', 
            background: 'var(--glass-highlight)', display: 'flex', alignItems: 'center', justifyContent: 'center' 
          }}>
            {getTrendIcon()}
          </div>
          <div>
            <div style={{ fontSize: '0.8rem', color: 'var(--text-muted)' }}>Current Trend</div>
            <div style={{ fontWeight: '600' }}>{trend}</div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default StatusBadge;
