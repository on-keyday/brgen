import {IWorker, JobManager} from "./job_mgr.js";


type WorkerMap= Readonly<{[key :string]:()=>IWorker }>;

export const WorkerFactory = class {

    #jobs = new Map<string,JobManager>();

    #workerCandidates : WorkerMap[] = [];

    addWorker = (worker :WorkerMap) => {
        this.#workerCandidates.push(worker)
    }
    

    getWorker = (lang :string) => {
        let mgr = this.#jobs.get(lang);
        if(mgr) return mgr;
        for(const i in this.#workerCandidates) {
            if(Object.hasOwn(this.#workerCandidates[i],lang)) {
                const worker = this.#workerCandidates[i][lang]();
                mgr = new JobManager(worker);
                this.#jobs.set(lang,mgr);
                return mgr;
            }
        }
        throw Error(`unsupported worker: ${lang}`)
    }

  
}
