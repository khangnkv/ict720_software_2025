db.createUser({
    user: "dbuser",
    pwd: "dbpasswd",
    roles: [{
        role: "readWrite",
        db: "babyCare_db"
    }]
});