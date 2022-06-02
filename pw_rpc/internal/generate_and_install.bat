protoc packet.proto --python_out=./py/pw_rpc/internal
cd py
python setup.py install