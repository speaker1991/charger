/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        brand: {
          50: '#E9F5F6',
          100: '#CBEBEB',
          200: '#A4DEE0',
          300: '#73CDD0',
          400: '#40B9BD',
          500: '#10A8AB',
          600: '#0F888B',
          700: '#0E6B6C',
          800: '#0C5355',
          900: '#035062',
          DEFAULT: '#10A8AB',
          light: '#E9F5F6',
          dark: '#035062',
        }
      }
    },
  },
  plugins: [],
}
