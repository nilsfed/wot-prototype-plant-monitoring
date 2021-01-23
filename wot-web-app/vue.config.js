require('dotenv').config();
const fs = require('fs')

module.exports = {
  devServer: {
    // proxy: 'http://192.168.178.62' //für CORS?
    port: process.env.VUE_APP_PORT,
    disableHostCheck: true,
    https: {
      cert: fs.readFileSync(process.env.VUE_APP_CERT_PATH),
      key: fs.readFileSync(process.env.VUE_APP_PRIVATE_KEY_PATH),
    },
    public: 'https://plantfriend.ddns.net/'
  },
  "transpileDependencies": [
    "vuetify"
  ],
  publicPath: ''
}
