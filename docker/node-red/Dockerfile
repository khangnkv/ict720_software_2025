FROM nodered/node-red:latest

# Install Node-RED nodes first
RUN npm install node-red-dashboard && \
    npm install node-red-node-mongodb && \
    npm install node-red-contrib-mqtt-broker && \
    npm install node-red-contrib-telegrambot


USER root
RUN mkdir -p /data && chown -R node-red:node-red /data
USER node-red
EXPOSE 1880