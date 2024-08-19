FROM python:3

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    libffi-dev \
    libssl-dev

# Install Python packages
COPY requirements.txt /etc/logzilla/scripts/.compliance.requirements.txt
RUN pip install --no-cache-dir -r /etc/logzilla/scripts/.compliance.requirements.txt

# Copy the Python script and configuration file to the container
COPY compliance.py /etc/logzilla/scripts/compliance
COPY compliance.yaml /etc/logzilla/scripts/.compliance.yaml

# Set the working directory
WORKDIR /etc/logzilla/scripts

# Default command to run the script
ENTRYPOINT ["python", "/etc/logzilla/scripts/compliance"]

