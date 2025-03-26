FROM nodered/node-red:latest

# Install required Node-RED modules for your baby monitor project
RUN npm install --no-cache-dir --unsafe-perm node-red-dashboard && \
    npm install --no-cache-dir --unsafe-perm node-red-node-mongodb

# Expose the Node-RED port
EXPOSE 1880