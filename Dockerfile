FROM alpine:latest

RUN apk add --no-cache gcc musl-dev make

WORKDIR /app
COPY . .

RUN make

EXPOSE 8080

CMD ["./mini-webserver", "8080", "/app/public"]
