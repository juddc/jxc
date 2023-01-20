import os
import argparse
from jxdocgen import (docgen, siteconfig, search)
import logging


logging.basicConfig(level=logging.INFO)
dev_server_log = logging.getLogger(os.path.basename(__file__))


def build_docs(cfg_path: str, docs_dir: str, output_dir: str):
    docgen.build_docs(cfg_path, docs_dir, output_dir)



def run_dev_server(listen_host: str, listen_port: int, cfg_path: str, docs_dir: str, output_dir: str):
    from jxdocgen import devserver

    site: siteconfig.SiteGenerator = docgen.build_docs(cfg_path, docs_dir, output_dir)
    watch_paths = [docs_dir, docgen.get_template_dir(site), docgen.get_static_dir(site), cfg_path]

    for path in docgen.get_all_static_files(site, None):
        watch_paths.append(path)

    def rebuild_docs_callback():
        dev_server_log.info("Source changed, regenerating...")
        site = docgen.build_docs(cfg_path, docs_dir, output_dir)

    server = devserver.LiveReloadServer(rebuild_docs_callback, listen_host, listen_port, root=args.output)

    for path in watch_paths:
        server.watch(path)

    try:
        server.serve()
    except KeyboardInterrupt:
        dev_server_log.info("Got KeyboardInterrupt, shutting down dev server...")
        for path in watch_paths:
            server.unwatch(path)
        server.shutdown(wait=True)
    except Exception as e:
        dev_server_log.error(f"Got exception {e}, shutting down dev server...")
        for path in watch_paths:
            server.unwatch(path)
        server.shutdown(wait=False)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', '-i', type=str, required=True,
        help='Documentation source directory')
    parser.add_argument('--config', '-c', type=str,
        help='Documentation config JXC file path '
            '(required if the source directory does not contain a file named "jxdocgen.config.jxc")')
    parser.add_argument('--output', '-o', type=str, required=True,
        help='Documentation build output directory')
    parser.add_argument('--dev-server', '-d', action='store_true',
        help='Starts a development server (defaults to localhost:8080, see --listen-host and --listen-port to change)')
    parser.add_argument('--listen-host', '-l', type=str, default='localhost',
        help='Development server listen host (requires --dev-server)')
    parser.add_argument('--listen-port', '-p', type=int, default=8080,
        help='Development server listen port (requires --dev-server)')
    args = parser.parse_args()

    docs_dir = os.path.abspath(args.input)

    if not os.path.exists(docs_dir):
        raise FileNotFoundError(docs_dir)
    
    if args.config:
        cfg_path = os.path.abspath(docs_dir)
    else:
        cfg_path = os.path.abspath(os.path.join(docs_dir, 'jxdocgen.config.jxc'))

    if not os.path.exists(cfg_path):
        raise FileNotFoundError(cfg_path)

    if args.dev_server:
        run_dev_server(args.listen_host, args.listen_port, cfg_path, docs_dir, args.output)
    else:
        build_docs(cfg_path, docs_dir, args.output)

