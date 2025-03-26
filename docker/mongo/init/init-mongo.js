db.createUser({
    user: "dbuser",
    pwd: "dbpasswd",
    roles: [{
        role: "readWrite",
        db: "babyMonitor_db"
    }]
});