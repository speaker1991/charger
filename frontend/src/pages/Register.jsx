import React, { useState, useEffect } from 'react';
import { useAuth } from '../components/AuthContext';
import { UserPlus, MapPin } from 'lucide-react';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { api } from '../api';

export default function Register() {
  const [searchParams] = useSearchParams();
  const token = searchParams.get('token');
  
  const [formData, setFormData] = useState({
    email: '',
    password: '',
    full_name: '',
    phone: '',
  });
  const [error, setError] = useState('');
  const [inviteData, setInviteData] = useState(null);
  const [isLoading, setIsLoading] = useState(false);
  const [isValidating, setIsValidating] = useState(true);
  const { register } = useAuth();
  const navigate = useNavigate();

  useEffect(() => {
    if (!token) {
      setError('Отсутствует токен приглашения. Регистрация доступна только по ссылке.');
      setIsValidating(false);
      return;
    }

    api.get(`/auth/validate-invite?token=${token}`)
      .then(res => {
        if (!res.valid) {
          setError('Ссылка приглашения недействительна или уже была использована.');
        } else {
          setInviteData(res);
        }
      })
      .catch(() => setError('Ошибка при проверке ссылки приглашения.'))
      .finally(() => setIsValidating(false));
  }, [token]);

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!inviteData) return;
    
    setError('');
    setIsLoading(true);
    try {
      await register({ ...formData, invite_token: token });
      navigate('/app');
    } catch (err) {
      setError(err.message || 'Ошибка регистрации');
      setIsLoading(false);
    }
  };

  const handleChange = (e) => {
    setFormData(prev => ({ ...prev, [e.target.name]: e.target.value }));
  };

  if (isValidating) {
    return <div className="min-h-screen bg-slate-50 flex items-center justify-center font-bold text-slate-500">Проверка приглашения...</div>;
  }

  return (
    <div className="min-h-screen bg-slate-50 flex items-center justify-center p-4">
      <div className="w-full max-w-lg bg-white rounded-3xl shadow-xl overflow-hidden">
        <div className="bg-brand p-8 text-center relative overflow-hidden">
          <div className="w-16 h-16 bg-white/20 backdrop-blur rounded-2xl flex items-center justify-center mx-auto mb-4 border border-white/30 text-white shadow-lg">
            <UserPlus size={32} />
          </div>
          <h1 className="text-2xl font-black text-white mb-2">Создание аккаунта</h1>
          {inviteData && (
            <p className="text-brand-100 text-sm font-medium">
              Доступ: <span className="uppercase tracking-wider font-bold text-white bg-white/20 px-2 py-0.5 rounded ml-1">{inviteData.target_role}</span>
            </p>
          )}
        </div>

        <div className="p-8">
          {error ? (
            <div className="text-center space-y-6">
              <div className="bg-rose-50 text-rose-500 p-6 rounded-2xl text-sm font-bold border border-rose-100">
                {error}
              </div>
              <button 
                onClick={() => navigate('/login')}
                className="text-brand font-bold hover:underline"
              >
                Вернуться к входу
              </button>
            </div>
          ) : (
            <form onSubmit={handleSubmit} className="space-y-6">
              <div className="space-y-4">
                <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
                  <div>
                    <label className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 block">Email</label>
                    <input
                      name="email" type="email" required value={formData.email} onChange={handleChange}
                      className="w-full px-4 py-3 bg-slate-50 border-2 border-slate-100 rounded-xl focus:border-brand-300 focus:bg-white transition-all font-medium"
                      placeholder="mail@example.com"
                    />
                  </div>
                  <div>
                    <label className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 block">Пароль</label>
                    <input
                      name="password" type="password" required minLength={6} value={formData.password} onChange={handleChange}
                      className="w-full px-4 py-3 bg-slate-50 border-2 border-slate-100 rounded-xl focus:border-brand-300 focus:bg-white transition-all font-medium"
                      placeholder="••••••••"
                    />
                  </div>
                </div>

                <div>
                  <label className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 block">ФИО</label>
                  <input
                    name="full_name" type="text" required value={formData.full_name} onChange={handleChange}
                    className="w-full px-4 py-3 bg-slate-50 border-2 border-slate-100 rounded-xl focus:border-brand-300 focus:bg-white transition-all font-medium"
                    placeholder="Иван Иванов"
                  />
                </div>

                <div>
                  <label className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 block">Телефон</label>
                  <input
                    name="phone" type="tel" value={formData.phone} onChange={handleChange}
                    className="w-full px-4 py-3 bg-slate-50 border-2 border-slate-100 rounded-xl focus:border-brand-300 focus:bg-white transition-all font-medium"
                    placeholder="+7 (999) 000-00-00"
                  />
                </div>
              </div>

              {inviteData?.station_ids?.length > 0 && (
                <div className="bg-slate-50 p-4 rounded-xl border border-slate-100">
                  <div className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 flex items-center gap-1.5">
                    <MapPin size={12}/> Доступные станции
                  </div>
                  <div className="flex flex-wrap gap-2">
                    {inviteData.station_ids.map(id => (
                      <span key={id} className="text-[10px] font-bold text-brand-700 bg-brand-50 px-2 py-1 rounded border border-brand-100">
                        ЭЗС #{id}
                      </span>
                    ))}
                  </div>
                </div>
              )}

              <button
                type="submit" disabled={isLoading}
                className="w-full bg-brand text-white font-bold text-sm py-4 rounded-xl shadow-lg shadow-brand-200 hover:bg-brand-dark transition-all uppercase tracking-wider disabled:opacity-70"
              >
                {isLoading ? 'Создание...' : 'Завершить регистрацию'}
              </button>
            </form>
          )}
        </div>
      </div>
    </div>
  );
}
