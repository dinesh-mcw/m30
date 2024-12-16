"""WSGI entry point for Gunicorn"""
from cobra_lidar_api.web_server import create_app

app = create_app()

if __name__ == '__main__':

    app.run('0.0.0.0', port=5001)
