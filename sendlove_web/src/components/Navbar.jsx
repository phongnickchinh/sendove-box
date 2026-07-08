import { Link, useLocation, useNavigate } from 'react-router-dom';
import { logOut } from '../api/auth';

export default function Navbar({ user }) {
  const location = useLocation();
  const navigate = useNavigate();

  const handleLogout = async () => {
    await logOut();
    navigate('/');
  };

  const navItemStyle = (path) => ({
    padding: '8px 16px',
    borderRadius: 'var(--radius-pill)',
    fontWeight: location.pathname === path ? 600 : 400,
    background: location.pathname === path ? 'rgba(255, 117, 140, 0.15)' : 'transparent',
    color: location.pathname === path ? 'var(--color-primary)' : 'var(--color-text-main)',
    transition: 'var(--transition-smooth)'
  });

  return (
    <nav className="glass-panel slide-up" style={{
      margin: '20px',
      padding: '16px 24px',
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      position: 'sticky',
      top: '20px',
      zIndex: 100
    }}>
      {/* Brand */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '24px' }}>
        <Link to="/dashboard" className="text-gradient" style={{ 
          fontFamily: 'var(--font-heading)', 
          fontSize: '1.8rem', 
          fontWeight: 700 
        }}>
          Sendlove
        </Link>
      </div>

      {/* User Actions */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ fontSize: '0.9rem', fontWeight: 500 }}>{user?.displayName}</span>
          {user?.photoURL && (
            <img 
              src={user.photoURL} 
              alt="avatar" 
              style={{ width: '36px', height: '36px', borderRadius: '50%', objectFit: 'cover' }} 
            />
          )}
          <button
            onClick={handleLogout}
            style={{
              background: 'none',
              border: '1px solid var(--color-glass-border)',
              padding: '6px 14px',
              borderRadius: 'var(--radius-pill)',
              cursor: 'pointer',
              fontSize: '0.85rem',
              fontWeight: 500,
              color: 'var(--color-text-muted)',
              transition: 'var(--transition-smooth)',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = 'rgba(255, 59, 48, 0.1)';
              e.currentTarget.style.color = '#ff3b30';
              e.currentTarget.style.borderColor = '#ff3b30';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = 'none';
              e.currentTarget.style.color = 'var(--color-text-muted)';
              e.currentTarget.style.borderColor = 'var(--color-glass-border)';
            }}
          >
            Đăng xuất
          </button>
        </div>
      </div>
    </nav>
  );
}
