import React, { useState } from 'react';
import { useAuth } from '../components/AuthContext';
import { Power, Zap } from 'lucide-react';
import { useNavigate } from 'react-router-dom';

export default function Login() {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const { login } = useAuth();
  const navigate = useNavigate();

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError('');
    setIsLoading(true);
    try {
      await login(email, password);
      // Wait for AuthContext to update and redirect naturally via App.jsx guards or navigate here
      navigate('/app');
    } catch (err) {
      setError(err.message || 'Ошибка входа');
      setIsLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-slate-50 flex items-center justify-center p-4">
      <div className="w-full max-w-md bg-white rounded-3xl shadow-xl overflow-hidden">
        <div className="bg-brand p-8 text-center relative overflow-hidden">
          <div className="absolute top-0 right-0 p-4 opacity-10">
            <Zap size={100} />
          </div>
          <div className="w-16 h-16 bg-white/20 backdrop-blur rounded-2xl flex items-center justify-center mx-auto mb-4 border border-white/30 text-white shadow-lg">
            <Power size={32} />
          </div>
          <h1 className="text-2xl font-black text-white mb-2">Vincoder Network</h1>
          <p className="text-brand-100 text-sm font-medium">Закрытая система управления активами</p>
        </div>

        <form onSubmit={handleSubmit} className="p-8 space-y-6">
          {error && (
            <div className="bg-rose-50 text-rose-500 p-4 rounded-xl text-sm font-semibold border border-rose-100 flex items-center gap-2">
              <div className="w-1.5 h-1.5 bg-rose-500 rounded-full" />
              {error}
            </div>
          )}

          <div className="space-y-4">
            <div>
              <label className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 block">
                Email
              </label>
              <input
                type="email"
                required
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                className="w-full px-4 py-3 bg-slate-50 border-2 border-slate-100 rounded-xl focus:outline-none focus:border-brand-300 focus:bg-white transition-all text-slate-800 font-medium"
                placeholder="admin@vincoder.com"
              />
            </div>

            <div>
              <label className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 block">
                Пароль
              </label>
              <input
                type="password"
                required
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                className="w-full px-4 py-3 bg-slate-50 border-2 border-slate-100 rounded-xl focus:outline-none focus:border-brand-300 focus:bg-white transition-all text-slate-800 font-medium"
                placeholder="••••••••"
              />
            </div>
          </div>

          <button
            type="submit"
            disabled={isLoading}
            className="w-full bg-brand text-white font-bold text-sm py-4 rounded-xl shadow-lg shadow-brand-200 hover:bg-brand-dark hover:shadow-xl hover:-translate-y-0.5 active:translate-y-0 transition-all disabled:opacity-70 disabled:hover:translate-y-0 uppercase tracking-wider"
          >
            {isLoading ? 'Вход...' : 'Войти в систему'}
          </button>
        </form>
      </div>
    </div>
  );
}
