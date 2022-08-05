FROM python:3.8-slim-buster

RUN apt-get update && apt-get install -y \
  ffmpeg \
  libsm6 \
  libxext6 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt requirements.txt
RUN pip3 install -r requirements.txt

COPY . .

CMD [ "python3", "example.py" ]