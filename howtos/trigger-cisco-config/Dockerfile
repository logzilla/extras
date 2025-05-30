# Use a logzilla script-server base image
FROM logzilla/script-server:latest

# Copy the requirements.txt file to the container
COPY requirements.txt /tmp/requirements.txt

# Install Python dependencies
RUN pip install -r /tmp/requirements.txt \
    --no-cache-dir --break-system-packages --root-user-action=ignore

# Copy script content to the container
RUN mkdir -p /scripts
COPY compliance.py /scripts
COPY compliance.yaml /scripts
RUN chmod +x /scripts/compliance.py
