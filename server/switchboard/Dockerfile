FROM python:3.12-slim
RUN pip install --no-cache-dir flask
WORKDIR /app
COPY app.py .
ENV SWITCHBOARD_DB=/data/switchboard.db ASTERISK_CONF_DIR=/asterisk-conf
CMD ["python3", "app.py"]
