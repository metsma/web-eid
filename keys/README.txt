Certificate comes from Letsencrypt.

certbot certonly -d app.web-eid.com --manual  --preferred-challenges=dns

python obf.py X.pem
cp X.pem.bin X
