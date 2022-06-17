sudo cp redisBrain.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart redisBrain
sudo systemctl status redisBrain
sudo systemctl enable redisBrain

